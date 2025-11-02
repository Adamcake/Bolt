#include "app.hxx"
#include "include/cef_process_message.h"
#include "include/cef_v8.h"
#include "include/cef_values.h"
#include "include/internal/cef_types.h"

/*
The functions in this file run in chromium's render process, which is subject to the full extent
of its sandboxing measures. The seccomp sandbox in particular is the one to watch out for. Don't
do anything that might lead to a syscall, so that means no file operations, no stdout etc. no
IPC, no sockets, no downloading. Except of course when using CEF's API as documented.

Of course, there is no seccomp on windows, which is why the OpenFileMapping stuff is able to work.
Yes, you read that right,it's theoretically possible for web pages in chromium-based browsers to
access your whole filesystem on a windows PC, using OpenFileMappingW. Do try not to think about it.
*/

#if defined(_WIN32)
#include <Windows.h>
static HANDLE shm_handle;
#else
#include <sys/mman.h>
#include <fcntl.h>
static int shm_fd;
#endif
static void* shm_file;
static size_t shm_length;
static bool shm_inited = false;

static bool set_launcher_ui = false;
static bool has_custom_js = false;
static CefString custom_js;

// assumes the context has already been entered
void PostPluginMessage(CefRefPtr<CefV8Context> context, CefRefPtr<CefProcessMessage> message) {
	CefRefPtr<CefV8Value> post_message = context->GetGlobal()->GetValue("postMessage");
	if (post_message->IsFunction()) {
		CefRefPtr<CefListValue> list = message->GetArgumentList();
		for (size_t i = 0; i < list->GetSize(); i += 1) {
			// "data" should be the exact contents of the lua string, since lua strings are
			// just byte arrays with no encoding or anything
			CefRefPtr<CefBinaryValue> data = list->GetBinary(i);
			size_t size = data->GetSize();
			void* buffer = malloc(size);
			data->GetData(buffer, size, 0);
			CefRefPtr<CefV8Value> content = CefV8Value::CreateArrayBufferWithCopy(buffer, size);
			free(buffer);

			// equivalent to: `window.postMessage({type: 'pluginMessage', content: ArrayBuffer...}, '*')`
			CefRefPtr<CefV8Value> dict = CefV8Value::CreateObject(nullptr, nullptr);
			dict->SetValue("type", CefV8Value::CreateString("pluginMessage"), V8_PROPERTY_ATTRIBUTE_READONLY);
			dict->SetValue("content", content, V8_PROPERTY_ATTRIBUTE_READONLY);
			CefV8ValueList value_list = {dict, CefV8Value::CreateString("*")};
			post_message->ExecuteFunctionWithContext(context, nullptr, value_list);
		}
	}
}

Browser::App::App(): browser_process_handler(nullptr), loaded(false) {
	
}

void Browser::App::SetBrowserProcessHandler(CefRefPtr<CefBrowserProcessHandler> handler) {
	this->browser_process_handler = handler;
}

CefRefPtr<CefRenderProcessHandler> Browser::App::GetRenderProcessHandler() {
	return this;
}

CefRefPtr<CefBrowserProcessHandler> Browser::App::GetBrowserProcessHandler() {
	// May be null, but in theory it should never be null on the main thread because it gets set via
	// SetBrowserProcessHandler() before CefInitialize() is called. In other processes it will be null.
	return this->browser_process_handler;
}

CefRefPtr<CefLoadHandler> Browser::App::GetLoadHandler() {
	return this;
}

void Browser::App::OnBrowserCreated(CefRefPtr<CefBrowser> browser, CefRefPtr<CefDictionaryValue> dict) {
	if (dict) {
		if (dict->HasKey("launcher")) set_launcher_ui = dict->GetBool("launcher");
		if (dict->HasKey("customjs")) {
			custom_js = dict->GetString("customjs");
			has_custom_js = true;
		}
	}
}

void Browser::App::OnContextCreated(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame, CefRefPtr<CefV8Context> context) {
	const CefRefPtr<CefV8Value> global = context->GetGlobal();
	global->SetValue("close", CefV8Value::CreateUndefined(), V8_PROPERTY_ATTRIBUTE_READONLY);
	if (set_launcher_ui) {
		global->SetValue("s", CefV8Value::CreateFunction("s", this), V8_PROPERTY_ATTRIBUTE_READONLY);
	}
	if (has_custom_js) {
		frame->ExecuteJavaScript(custom_js, CefString(), int());
	}
}

void Browser::App::OnUncaughtException(
	CefRefPtr<CefBrowser>,
	CefRefPtr<CefFrame> frame,
	CefRefPtr<CefV8Context>,
	CefRefPtr<CefV8Exception> exception,
	CefRefPtr<CefV8StackTrace> trace
) {
	CefRefPtr<CefProcessMessage> message = CefProcessMessage::Create("__bolt_exception");
	const int frame_count = trace->GetFrameCount();
	CefRefPtr<CefListValue> args = message->GetArgumentList();
	args->SetSize(frame_count + 5);
	args->SetString(0, exception->GetScriptResourceName());
	args->SetString(1, exception->GetMessage());
	args->SetString(2, exception->GetSourceLine());
	args->SetInt(3, exception->GetStartColumn());
	args->SetInt(4, exception->GetEndColumn());
	for (int i = 0; i < frame_count; i += 1) {
		const CefRefPtr<CefV8StackFrame> frame = trace->GetFrame(i);
		CefRefPtr<CefListValue> frame_details = CefListValue::Create();
		frame_details->SetSize(3);
		frame_details->SetString(0, frame->GetScriptName());
		frame_details->SetInt(1, frame->GetLineNumber());
		frame_details->SetInt(2, frame->GetColumn());
		args->SetList(i + 5, frame_details);
	}
	frame->SendProcessMessage(PID_BROWSER, message);
}

bool Browser::App::OnProcessMessageReceived(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame, CefProcessId, CefRefPtr<CefProcessMessage> message) {
	CefString name = message->GetName();

	if (name == "__bolt_pluginbrowser_close") {
		// pinging a message from browser -> renderer -> browser is the only way I can find of delaying a task
		// without being on a different thread than the one I want to run it on. in this case, destroying a
		// browser while still in the Create function of that browser. this would cause a segfault if we tried
		// to do it directly so it needs to be delayed until some arbitrary later time. so, in that specific
		// case, a message is sent here and we send the same message straight back.
		frame->SendProcessMessage(PID_BROWSER, message);
		return true;
	}

	if (name == "__bolt_clientlist") {
		CefRefPtr<CefV8Context> context = frame->GetV8Context();
		context->Enter();
		CefRefPtr<CefV8Value> post_message = context->GetGlobal()->GetValue("postMessage");
		if (post_message->IsFunction()) {
			CefRefPtr<CefListValue> list = message->GetArgumentList();
			CefRefPtr<CefV8Value> clients = CefV8Value::CreateArray(list->GetSize());

			for (size_t i = 0; i < list->GetSize(); i += 1) {
				CefRefPtr<CefDictionaryValue> dict = list->GetDictionary(i);
				CefRefPtr<CefV8Value> client = CefV8Value::CreateObject(nullptr, nullptr);
				CefRefPtr<CefListValue> plugin_list = dict->GetList("plugins");
				CefRefPtr<CefV8Value> plugins = CefV8Value::CreateArray(plugin_list->GetSize());

				for (size_t j = 0; j < plugin_list->GetSize(); j += 1) {
					CefRefPtr<CefDictionaryValue> in_plugin = plugin_list->GetDictionary(j);
					CefRefPtr<CefV8Value> plugin = CefV8Value::CreateObject(nullptr, nullptr);
					plugin->SetValue("id", CefV8Value::CreateString(in_plugin->GetString("id")), V8_PROPERTY_ATTRIBUTE_READONLY);
					plugin->SetValue("uid", CefV8Value::CreateUInt(in_plugin->GetInt("uid")), V8_PROPERTY_ATTRIBUTE_READONLY);
					plugins->SetValue(j, plugin);
				}

				if (dict->HasKey("identity")) {
					client->SetValue("identity", CefV8Value::CreateString(dict->GetString("identity")), V8_PROPERTY_ATTRIBUTE_READONLY);
				}
				client->SetValue("uid", CefV8Value::CreateUInt(dict->GetInt("uid")), V8_PROPERTY_ATTRIBUTE_READONLY);
				client->SetValue("plugins", plugins, V8_PROPERTY_ATTRIBUTE_READONLY);

				clients->SetValue(i, client);
			}

			// equivalent to: `window.postMessage({type: 'gameClientList', clients: [ ... ]}, '*')`
			CefRefPtr<CefV8Value> dict = CefV8Value::CreateObject(nullptr, nullptr);
			dict->SetValue("type", CefV8Value::CreateString("gameClientList"), V8_PROPERTY_ATTRIBUTE_READONLY);
			dict->SetValue("clients", clients, V8_PROPERTY_ATTRIBUTE_READONLY);
			CefV8ValueList value_list = {dict, CefV8Value::CreateString("*")};
			post_message->ExecuteFunctionWithContext(context, nullptr, value_list);
		}
		context->Exit();
		return true;
	}

	if (name == "__bolt_plugin_message") {
		if (this->loaded) {
			CefRefPtr<CefV8Context> context = frame->GetV8Context();
			context->Enter();
			PostPluginMessage(context, message);
			context->Exit();
		} else {
			this->pending_plugin_messages.push_back(message);
		}
		return true;
	}

	if (name == "__bolt_plugin_capture") {
		if (!this->loaded) {
			CefRefPtr<CefProcessMessage> response_message = CefProcessMessage::Create("__bolt_plugin_capture_done");
			frame->SendProcessMessage(PID_BROWSER, response_message);
			return true;
		}

		CefRefPtr<CefListValue> list = message->GetArgumentList();
		if (list->GetSize() >= 2) {
			const int width = list->GetInt(0);
			const int height = list->GetInt(1);
			const size_t size = (size_t)width * (size_t)height * 3;

			if (list->GetSize() != 2) {
#if defined(_WIN32)
				const std::wstring path = list->GetString(2).ToWString();
				if (shm_inited) {
					UnmapViewOfFile(shm_file);
					CloseHandle(shm_handle);
				}
				shm_handle = OpenFileMappingW(FILE_MAP_READ, TRUE, path.c_str());
				shm_file = MapViewOfFile(shm_handle, FILE_MAP_READ, 0, 0, size);
#else
				if (list->GetType(2) == VTYPE_STRING) {
					const std::string path = list->GetString(2).ToString();
					if (shm_inited) {
						munmap(shm_file, shm_length);
						close(shm_fd);
					}
					shm_fd = shm_open(path.c_str(), O_RDWR, 0644);
					shm_file = mmap(NULL, size, PROT_READ, MAP_SHARED, shm_fd, 0);
				} else if (shm_inited) {
					shm_file = mremap(shm_file, shm_length, size, MREMAP_MAYMOVE);
				} else {
					// shm not set up and wasn't provided with enough information to do setup - shouldn't happen
					return true;
				}
#endif
				shm_length = size;
				shm_inited = true;
			}

			void* buffer = malloc(size);
			memcpy(buffer, shm_file, size);

			CefRefPtr<CefProcessMessage> response_message = CefProcessMessage::Create("__bolt_plugin_capture_done");
			frame->SendProcessMessage(PID_BROWSER, response_message);

			CefRefPtr<CefV8Context> context = frame->GetV8Context();
			context->Enter();
			CefRefPtr<CefV8Value> content = CefV8Value::CreateArrayBufferWithCopy(buffer, size);
			free(buffer);
			CefRefPtr<CefV8Value> dict = CefV8Value::CreateObject(nullptr, nullptr);
			dict->SetValue("type", CefV8Value::CreateString("screenCapture"), V8_PROPERTY_ATTRIBUTE_READONLY);
			dict->SetValue("content", content, V8_PROPERTY_ATTRIBUTE_READONLY);
			dict->SetValue("width", CefV8Value::CreateInt(width), V8_PROPERTY_ATTRIBUTE_READONLY);
			dict->SetValue("height", CefV8Value::CreateInt(height), V8_PROPERTY_ATTRIBUTE_READONLY);
			CefV8ValueList value_list = {dict, CefV8Value::CreateString("*")};
			CefRefPtr<CefV8Value> post_message = context->GetGlobal()->GetValue("postMessage");
			if (post_message->IsFunction()) {
				post_message->ExecuteFunctionWithContext(context, nullptr, value_list);
			}
			context->Exit();
		}

		return true;
	}

	return false;
}

void Browser::App::OnLoadEnd(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame, int) {
	if (CefCurrentlyOn(TID_RENDERER)) {
		if (frame->IsMain()) {
			this->loaded = true;
			if (this->pending_plugin_messages.size()) {
				CefRefPtr<CefV8Context> context = frame->GetV8Context();
				context->Enter();
				for (const CefRefPtr<CefProcessMessage> message: this->pending_plugin_messages) {
					PostPluginMessage(context, message);
				}
				context->Exit();
				this->pending_plugin_messages.clear();
			}
		}
	}
}

void Browser::App::OnLoadError(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame, ErrorCode, const CefString&, const CefString&) {
	// TODO: this should probably be a fatal error for plugins? or at least inform them?
}

bool Browser::App::Execute(const CefString&, CefRefPtr<CefV8Value>, const CefV8ValueList&, CefRefPtr<CefV8Value>& retval, CefString&) {
	retval = CefV8Value::CreateObject(nullptr, nullptr);
	retval->SetValue("provider", CefV8Value::CreateString("cnVuZXNjYXBl"), V8_PROPERTY_ATTRIBUTE_READONLY);
	retval->SetValue("origin", CefV8Value::CreateString("aHR0cHM6Ly9hY2NvdW50LmphZ2V4LmNvbQ"), V8_PROPERTY_ATTRIBUTE_READONLY);
	retval->SetValue("origin_2fa", CefV8Value::CreateString("aHR0cHM6Ly9zZWN1cmUucnVuZXNjYXBlLmNvbQ"), V8_PROPERTY_ATTRIBUTE_READONLY);
	retval->SetValue("redirect", CefV8Value::CreateString("aHR0cHM6Ly9zZWN1cmUucnVuZXNjYXBlLmNvbS9tPXdlYmxvZ2luL2xhdW5jaGVyLXJlZGlyZWN0"), V8_PROPERTY_ATTRIBUTE_READONLY);
	retval->SetValue("clientid", CefV8Value::CreateString("Y29tX2phZ2V4X2F1dGhfZGVza3RvcF9sYXVuY2hlcg"), V8_PROPERTY_ATTRIBUTE_READONLY);
	retval->SetValue("api", CefV8Value::CreateString("aHR0cHM6Ly9hcGkuamFnZXguY29tL3Yx"), V8_PROPERTY_ATTRIBUTE_READONLY);
	retval->SetValue("auth_api", CefV8Value::CreateString("aHR0cHM6Ly9hdXRoLmphZ2V4LmNvbS9nYW1lLXNlc3Npb24vdjE"), V8_PROPERTY_ATTRIBUTE_READONLY);
	retval->SetValue("profile_api", CefV8Value::CreateString("aHR0cHM6Ly9zZWN1cmUuamFnZXguY29tL3JzLXByb2ZpbGUvdjE"), V8_PROPERTY_ATTRIBUTE_READONLY);
	retval->SetValue("shield_url", CefV8Value::CreateString("aHR0cHM6Ly9hdXRoLmphZ2V4LmNvbS9zaGllbGQvb2F1dGgvdG9rZW4"), V8_PROPERTY_ATTRIBUTE_READONLY);
	retval->SetValue("content_url", CefV8Value::CreateString("aHR0cHM6Ly9jb250ZW50LnJ1bmVzY2FwZS5jb20vZG93bmxvYWRzL3VidW50dS8"), V8_PROPERTY_ATTRIBUTE_READONLY);
	retval->SetValue("default_config_uri", CefV8Value::CreateString("aHR0cHM6Ly93d3cucnVuZXNjYXBlLmNvbS9rPTUvbD0wL2phdl9jb25maWcud3M"), V8_PROPERTY_ATTRIBUTE_READONLY);
	retval->SetValue("direct6_url", CefV8Value::CreateString("aHR0cHM6Ly9qYWdleC5ha2FtYWl6ZWQubmV0L2RpcmVjdDYv"), V8_PROPERTY_ATTRIBUTE_READONLY);
	retval->SetValue("psa_url", CefV8Value::CreateString("aHR0cHM6Ly9maWxlcy5wdWJsaXNoaW5nLnByb2R1Y3Rpb24uanhwLmphZ2V4LmNvbS8"), V8_PROPERTY_ATTRIBUTE_READONLY);

	CefRefPtr<CefV8Value> games = CefV8Value::CreateArray(2);
	games->SetValue(0, CefV8Value::CreateString("UnVuZVNjYXBl"));
	games->SetValue(1, CefV8Value::CreateString("T2xkIFNjaG9vbA"));
	retval->SetValue("games", games, V8_PROPERTY_ATTRIBUTE_READONLY);
	return true;
}

void Browser::App::OnRegisterCustomSchemes(CefRawPtr<CefSchemeRegistrar> registrar) {	
	registrar->AddCustomScheme("plugin", CEF_SCHEME_OPTION_CORS_ENABLED | CEF_SCHEME_OPTION_SECURE | CEF_SCHEME_OPTION_CSP_BYPASSING | CEF_SCHEME_OPTION_FETCH_ENABLED);
}
