#include "app.hxx"
#include "include/cef_base.h"
#include "include/cef_v8.h"
#include "include/cef_values.h"
#include "include/internal/cef_types.h"

#include <fmt/core.h>

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

class ArrayBufferReleaseCallbackFree: public CefV8ArrayBufferReleaseCallback {
	void ReleaseBuffer(void* buffer) override {
		::free(buffer);
	}
	IMPLEMENT_REFCOUNTING(ArrayBufferReleaseCallbackFree);
	DISALLOW_COPY_AND_ASSIGN(ArrayBufferReleaseCallbackFree);
	public:
		ArrayBufferReleaseCallbackFree() {}
};

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
			CefRefPtr<CefV8ArrayBufferReleaseCallback> cb = new ArrayBufferReleaseCallbackFree();
			void* buffer = malloc(size);
			data->GetData(buffer, size, 0);
			CefRefPtr<CefV8Value> content = CefV8Value::CreateArrayBuffer(buffer, size, cb);

			// equivalent to: `window.postMessage({type: 'pluginMessage', content: ArrayBuffer...}, '*')`
			CefRefPtr<CefV8Value> dict = CefV8Value::CreateObject(nullptr, nullptr);
			dict->SetValue("type", CefV8Value::CreateString("pluginMessage"), V8_PROPERTY_ATTRIBUTE_READONLY);
			dict->SetValue("content", content, V8_PROPERTY_ATTRIBUTE_READONLY);
			CefV8ValueList value_list = {dict, CefV8Value::CreateString("*")};
			post_message->ExecuteFunctionWithContext(context, nullptr, value_list);
		}
	} else {
		fmt::print("[R] warning: window.postMessage is not a function, plugin message will be ignored\n");
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
	fmt::print("[R] OnBrowserCreated for browser {}\n", browser->GetIdentifier());
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
	CefRefPtr<CefFrame>,
	CefRefPtr<CefV8Context>,
	CefRefPtr<CefV8Exception> exception,
	CefRefPtr<CefV8StackTrace> trace
) {
	fmt::print("[R] unhandled exception in {}: {}\n", exception->GetScriptResourceName().ToString(), exception->GetMessage().ToString());
	for (int i = 0; i < trace->GetFrameCount(); i += 1) {
		CefRefPtr<CefV8StackFrame> frame = trace->GetFrame(i);
		fmt::print("[R] in {}:{}:{}\n", frame->GetScriptName().ToString(), frame->GetLineNumber(), frame->GetColumn());
	}

	constexpr int max_dist = 80;
	std::string source_line = exception->GetSourceLine().ToString();
	const size_t first_non_whitespace = source_line.find_first_not_of(" \t");
	const size_t last_non_whitespace = source_line.find_last_not_of(" \t");
	const std::string_view source_view_trimmed(source_line.begin() + first_non_whitespace, source_line.begin() + last_non_whitespace + 1);
	const int exc_start_column = exception->GetStartColumn() - first_non_whitespace;
	const int exc_end_column = exception->GetEndColumn() - first_non_whitespace;
	const bool do_trim_start = exc_start_column > max_dist;
	const bool do_trim_end = source_view_trimmed.size() - exc_end_column > max_dist;
	const std::string_view code_trimmed(
		source_view_trimmed.data() + (do_trim_start ? exc_start_column - max_dist : 0),
		source_view_trimmed.data() + (do_trim_end ? exc_end_column + max_dist : source_view_trimmed.size())
	);

	fmt::print("[R] {}{}{}\n[R] {}", do_trim_start ? "..." : "", code_trimmed, do_trim_end ? "..." : "", do_trim_start ? "---" : "");
	int i = 0;
	while (i < exc_start_column && i < max_dist) {
		fmt::print("-");
		i += 1;
	}
	i = 0;
	while (i < exc_end_column - exc_start_column) {
		fmt::print("^");
		i += 1;
	}
	fmt::print("\n");
}

bool Browser::App::OnProcessMessageReceived(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame, CefProcessId, CefRefPtr<CefProcessMessage> message) {
	CefString name = message->GetName();

	if (name == "__bolt_refresh" || name == "__bolt_new_client" || name == "__bolt_no_more_clients" || name == "__bolt_open_launcher" || name == "__bolt_pluginbrowser_close") {
		frame->SendProcessMessage(PID_BROWSER, message);
		return true;
	}

	if (name == "__bolt_clientlist") {
		fmt::print("[R] handling client list\n");
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
		} else {
			fmt::print("[R] warning: window.postMessage is not a function, {} will be ignored\n", name.ToString());
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
					fmt::print("[R] shm named-remap '{}' -> {}, {}\n", path, shm_fd, (unsigned long long)shm_file);
				} else if (shm_inited) {
					shm_file = mremap(shm_file, shm_length, size, MREMAP_MAYMOVE);
					fmt::print("[R] shm unnamed-remap -> {} ({})\n", (unsigned long long)shm_file, errno);
				} else {
					fmt::print("[R] warning: shm not set up and wasn't provided with enough information to do setup; {} will be ignored\n", name.ToString());
					return true;
				}
#endif
				shm_length = size;
				shm_inited = true;
			}

			CefRefPtr<CefV8ArrayBufferReleaseCallback> cb = new ArrayBufferReleaseCallbackFree();
			void* buffer = malloc(size);
			memcpy(buffer, shm_file, size);

			CefRefPtr<CefProcessMessage> response_message = CefProcessMessage::Create("__bolt_plugin_capture_done");
			frame->SendProcessMessage(PID_BROWSER, response_message);

			CefRefPtr<CefV8Context> context = frame->GetV8Context();
			context->Enter();
			CefRefPtr<CefV8Value> content = CefV8Value::CreateArrayBuffer(buffer, size, cb);
			CefRefPtr<CefV8Value> dict = CefV8Value::CreateObject(nullptr, nullptr);
			dict->SetValue("type", CefV8Value::CreateString("screenCapture"), V8_PROPERTY_ATTRIBUTE_READONLY);
			dict->SetValue("content", content, V8_PROPERTY_ATTRIBUTE_READONLY);
			dict->SetValue("width", CefV8Value::CreateInt(width), V8_PROPERTY_ATTRIBUTE_READONLY);
			dict->SetValue("height", CefV8Value::CreateInt(height), V8_PROPERTY_ATTRIBUTE_READONLY);
			CefV8ValueList value_list = {dict, CefV8Value::CreateString("*")};
			CefRefPtr<CefV8Value> post_message = context->GetGlobal()->GetValue("postMessage");
			if (post_message->IsFunction()) {
				post_message->ExecuteFunctionWithContext(context, nullptr, value_list);
			} else {
				fmt::print("[R] warning: window.postMessage is not a function, {} will be ignored\n", name.ToString());
			}
			context->Exit();
		} else {
			fmt::print("[R] warning: too few arguments, {} will be ignored\n", name.ToString());
		}

		return true;
	}

	return false;
}

void Browser::App::OnLoadEnd(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame, int) {
	if (CefCurrentlyOn(TID_RENDERER)) {
		if (frame->IsMain()) {
			fmt::print("[R] OnLoadEnd for browser {}\n", browser->GetIdentifier());
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
	fmt::print("[R] OnLoadError\n");
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
