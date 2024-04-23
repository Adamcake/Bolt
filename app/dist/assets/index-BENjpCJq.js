var __defProp = Object.defineProperty;
var __defNormalProp = (obj, key, value) => key in obj ? __defProp(obj, key, { enumerable: true, configurable: true, writable: true, value }) : obj[key] = value;
var __publicField = (obj, key, value) => {
  __defNormalProp(obj, typeof key !== "symbol" ? key + "" : key, value);
  return value;
};
(function polyfill() {
  const relList = document.createElement("link").relList;
  if (relList && relList.supports && relList.supports("modulepreload")) {
    return;
  }
  for (const link of document.querySelectorAll('link[rel="modulepreload"]')) {
    processPreload(link);
  }
  new MutationObserver((mutations) => {
    for (const mutation of mutations) {
      if (mutation.type !== "childList") {
        continue;
      }
      for (const node of mutation.addedNodes) {
        if (node.tagName === "LINK" && node.rel === "modulepreload")
          processPreload(node);
      }
    }
  }).observe(document, { childList: true, subtree: true });
  function getFetchOpts(link) {
    const fetchOpts = {};
    if (link.integrity)
      fetchOpts.integrity = link.integrity;
    if (link.referrerPolicy)
      fetchOpts.referrerPolicy = link.referrerPolicy;
    if (link.crossOrigin === "use-credentials")
      fetchOpts.credentials = "include";
    else if (link.crossOrigin === "anonymous")
      fetchOpts.credentials = "omit";
    else
      fetchOpts.credentials = "same-origin";
    return fetchOpts;
  }
  function processPreload(link) {
    if (link.ep)
      return;
    link.ep = true;
    const fetchOpts = getFetchOpts(link);
    fetch(link.href, fetchOpts);
  }
})();
function noop() {
}
function is_promise(value) {
  return !!value && (typeof value === "object" || typeof value === "function") && typeof /** @type {any} */
  value.then === "function";
}
function run(fn) {
  return fn();
}
function blank_object() {
  return /* @__PURE__ */ Object.create(null);
}
function run_all(fns) {
  fns.forEach(run);
}
function is_function(thing) {
  return typeof thing === "function";
}
function safe_not_equal(a, b) {
  return a != a ? b == b : a !== b || a && typeof a === "object" || typeof a === "function";
}
let src_url_equal_anchor;
function src_url_equal(element_src, url) {
  if (element_src === url)
    return true;
  if (!src_url_equal_anchor) {
    src_url_equal_anchor = document.createElement("a");
  }
  src_url_equal_anchor.href = url;
  return element_src === src_url_equal_anchor.href;
}
function is_empty(obj) {
  return Object.keys(obj).length === 0;
}
function subscribe(store, ...callbacks) {
  if (store == null) {
    for (const callback of callbacks) {
      callback(void 0);
    }
    return noop;
  }
  const unsub = store.subscribe(...callbacks);
  return unsub.unsubscribe ? () => unsub.unsubscribe() : unsub;
}
function get_store_value(store) {
  let value;
  subscribe(store, (_) => value = _)();
  return value;
}
function component_subscribe(component, store, callback) {
  component.$$.on_destroy.push(subscribe(store, callback));
}
function set_store_value(store, ret, value) {
  store.set(value);
  return ret;
}
function append(target, node) {
  target.appendChild(node);
}
function insert(target, node, anchor) {
  target.insertBefore(node, anchor || null);
}
function detach(node) {
  if (node.parentNode) {
    node.parentNode.removeChild(node);
  }
}
function destroy_each(iterations, detaching) {
  for (let i = 0; i < iterations.length; i += 1) {
    if (iterations[i])
      iterations[i].d(detaching);
  }
}
function element(name) {
  return document.createElement(name);
}
function text(data) {
  return document.createTextNode(data);
}
function space() {
  return text(" ");
}
function empty() {
  return text("");
}
function listen(node, event, handler, options) {
  node.addEventListener(event, handler, options);
  return () => node.removeEventListener(event, handler, options);
}
function attr(node, attribute, value) {
  if (value == null)
    node.removeAttribute(attribute);
  else if (node.getAttribute(attribute) !== value)
    node.setAttribute(attribute, value);
}
function children(element2) {
  return Array.from(element2.childNodes);
}
function set_data(text2, data) {
  data = "" + data;
  if (text2.data === data)
    return;
  text2.data = /** @type {string} */
  data;
}
function set_input_value(input, value) {
  input.value = value == null ? "" : value;
}
function custom_event(type, detail, { bubbles = false, cancelable = false } = {}) {
  return new CustomEvent(type, { detail, bubbles, cancelable });
}
let current_component;
function set_current_component(component) {
  current_component = component;
}
function get_current_component() {
  if (!current_component)
    throw new Error("Function called outside component initialization");
  return current_component;
}
function onMount(fn) {
  get_current_component().$$.on_mount.push(fn);
}
function afterUpdate(fn) {
  get_current_component().$$.after_update.push(fn);
}
function onDestroy(fn) {
  get_current_component().$$.on_destroy.push(fn);
}
function createEventDispatcher() {
  const component = get_current_component();
  return (type, detail, { cancelable = false } = {}) => {
    const callbacks = component.$$.callbacks[type];
    if (callbacks) {
      const event = custom_event(
        /** @type {string} */
        type,
        detail,
        { cancelable }
      );
      callbacks.slice().forEach((fn) => {
        fn.call(component, event);
      });
      return !event.defaultPrevented;
    }
    return true;
  };
}
const dirty_components = [];
const binding_callbacks = [];
let render_callbacks = [];
const flush_callbacks = [];
const resolved_promise = /* @__PURE__ */ Promise.resolve();
let update_scheduled = false;
function schedule_update() {
  if (!update_scheduled) {
    update_scheduled = true;
    resolved_promise.then(flush);
  }
}
function add_render_callback(fn) {
  render_callbacks.push(fn);
}
function add_flush_callback(fn) {
  flush_callbacks.push(fn);
}
const seen_callbacks = /* @__PURE__ */ new Set();
let flushidx = 0;
function flush() {
  if (flushidx !== 0) {
    return;
  }
  const saved_component = current_component;
  do {
    try {
      while (flushidx < dirty_components.length) {
        const component = dirty_components[flushidx];
        flushidx++;
        set_current_component(component);
        update(component.$$);
      }
    } catch (e) {
      dirty_components.length = 0;
      flushidx = 0;
      throw e;
    }
    set_current_component(null);
    dirty_components.length = 0;
    flushidx = 0;
    while (binding_callbacks.length)
      binding_callbacks.pop()();
    for (let i = 0; i < render_callbacks.length; i += 1) {
      const callback = render_callbacks[i];
      if (!seen_callbacks.has(callback)) {
        seen_callbacks.add(callback);
        callback();
      }
    }
    render_callbacks.length = 0;
  } while (dirty_components.length);
  while (flush_callbacks.length) {
    flush_callbacks.pop()();
  }
  update_scheduled = false;
  seen_callbacks.clear();
  set_current_component(saved_component);
}
function update($$) {
  if ($$.fragment !== null) {
    $$.update();
    run_all($$.before_update);
    const dirty = $$.dirty;
    $$.dirty = [-1];
    $$.fragment && $$.fragment.p($$.ctx, dirty);
    $$.after_update.forEach(add_render_callback);
  }
}
function flush_render_callbacks(fns) {
  const filtered = [];
  const targets = [];
  render_callbacks.forEach((c) => fns.indexOf(c) === -1 ? filtered.push(c) : targets.push(c));
  targets.forEach((c) => c());
  render_callbacks = filtered;
}
const outroing = /* @__PURE__ */ new Set();
let outros;
function group_outros() {
  outros = {
    r: 0,
    c: [],
    p: outros
    // parent group
  };
}
function check_outros() {
  if (!outros.r) {
    run_all(outros.c);
  }
  outros = outros.p;
}
function transition_in(block, local) {
  if (block && block.i) {
    outroing.delete(block);
    block.i(local);
  }
}
function transition_out(block, local, detach2, callback) {
  if (block && block.o) {
    if (outroing.has(block))
      return;
    outroing.add(block);
    outros.c.push(() => {
      outroing.delete(block);
      if (callback) {
        if (detach2)
          block.d(1);
        callback();
      }
    });
    block.o(local);
  } else if (callback) {
    callback();
  }
}
function handle_promise(promise, info) {
  const token = info.token = {};
  function update2(type, index, key, value) {
    if (info.token !== token)
      return;
    info.resolved = value;
    let child_ctx = info.ctx;
    if (key !== void 0) {
      child_ctx = child_ctx.slice();
      child_ctx[key] = value;
    }
    const block = type && (info.current = type)(child_ctx);
    let needs_flush = false;
    if (info.block) {
      if (info.blocks) {
        info.blocks.forEach((block2, i) => {
          if (i !== index && block2) {
            group_outros();
            transition_out(block2, 1, 1, () => {
              if (info.blocks[i] === block2) {
                info.blocks[i] = null;
              }
            });
            check_outros();
          }
        });
      } else {
        info.block.d(1);
      }
      block.c();
      transition_in(block, 1);
      block.m(info.mount(), info.anchor);
      needs_flush = true;
    }
    info.block = block;
    if (info.blocks)
      info.blocks[index] = block;
    if (needs_flush) {
      flush();
    }
  }
  if (is_promise(promise)) {
    const current_component2 = get_current_component();
    promise.then(
      (value) => {
        set_current_component(current_component2);
        update2(info.then, 1, info.value, value);
        set_current_component(null);
      },
      (error) => {
        set_current_component(current_component2);
        update2(info.catch, 2, info.error, error);
        set_current_component(null);
        if (!info.hasCatch) {
          throw error;
        }
      }
    );
    if (info.current !== info.pending) {
      update2(info.pending, 0);
      return true;
    }
  } else {
    if (info.current !== info.then) {
      update2(info.then, 1, info.value, promise);
      return true;
    }
    info.resolved = /** @type {T} */
    promise;
  }
}
function update_await_block_branch(info, ctx, dirty) {
  const child_ctx = ctx.slice();
  const { resolved } = info;
  if (info.current === info.then) {
    child_ctx[info.value] = resolved;
  }
  if (info.current === info.catch) {
    child_ctx[info.error] = resolved;
  }
  info.block.p(child_ctx, dirty);
}
function ensure_array_like(array_like_or_iterator) {
  return (array_like_or_iterator == null ? void 0 : array_like_or_iterator.length) !== void 0 ? array_like_or_iterator : Array.from(array_like_or_iterator);
}
function bind(component, name, callback) {
  const index = component.$$.props[name];
  if (index !== void 0) {
    component.$$.bound[index] = callback;
    callback(component.$$.ctx[index]);
  }
}
function create_component(block) {
  block && block.c();
}
function mount_component(component, target, anchor) {
  const { fragment, after_update } = component.$$;
  fragment && fragment.m(target, anchor);
  add_render_callback(() => {
    const new_on_destroy = component.$$.on_mount.map(run).filter(is_function);
    if (component.$$.on_destroy) {
      component.$$.on_destroy.push(...new_on_destroy);
    } else {
      run_all(new_on_destroy);
    }
    component.$$.on_mount = [];
  });
  after_update.forEach(add_render_callback);
}
function destroy_component(component, detaching) {
  const $$ = component.$$;
  if ($$.fragment !== null) {
    flush_render_callbacks($$.after_update);
    run_all($$.on_destroy);
    $$.fragment && $$.fragment.d(detaching);
    $$.on_destroy = $$.fragment = null;
    $$.ctx = [];
  }
}
function make_dirty(component, i) {
  if (component.$$.dirty[0] === -1) {
    dirty_components.push(component);
    schedule_update();
    component.$$.dirty.fill(0);
  }
  component.$$.dirty[i / 31 | 0] |= 1 << i % 31;
}
function init(component, options, instance2, create_fragment2, not_equal, props, append_styles = null, dirty = [-1]) {
  const parent_component = current_component;
  set_current_component(component);
  const $$ = component.$$ = {
    fragment: null,
    ctx: [],
    // state
    props,
    update: noop,
    not_equal,
    bound: blank_object(),
    // lifecycle
    on_mount: [],
    on_destroy: [],
    on_disconnect: [],
    before_update: [],
    after_update: [],
    context: new Map(options.context || (parent_component ? parent_component.$$.context : [])),
    // everything else
    callbacks: blank_object(),
    dirty,
    skip_bound: false,
    root: options.target || parent_component.$$.root
  };
  append_styles && append_styles($$.root);
  let ready = false;
  $$.ctx = instance2 ? instance2(component, options.props || {}, (i, ret, ...rest) => {
    const value = rest.length ? rest[0] : ret;
    if ($$.ctx && not_equal($$.ctx[i], $$.ctx[i] = value)) {
      if (!$$.skip_bound && $$.bound[i])
        $$.bound[i](value);
      if (ready)
        make_dirty(component, i);
    }
    return ret;
  }) : [];
  $$.update();
  ready = true;
  run_all($$.before_update);
  $$.fragment = create_fragment2 ? create_fragment2($$.ctx) : false;
  if (options.target) {
    if (options.hydrate) {
      const nodes = children(options.target);
      $$.fragment && $$.fragment.l(nodes);
      nodes.forEach(detach);
    } else {
      $$.fragment && $$.fragment.c();
    }
    if (options.intro)
      transition_in(component.$$.fragment);
    mount_component(component, options.target, options.anchor);
    flush();
  }
  set_current_component(parent_component);
}
class SvelteComponent {
  constructor() {
    /**
     * ### PRIVATE API
     *
     * Do not use, may change at any time
     *
     * @type {any}
     */
    __publicField(this, "$$");
    /**
     * ### PRIVATE API
     *
     * Do not use, may change at any time
     *
     * @type {any}
     */
    __publicField(this, "$$set");
  }
  /** @returns {void} */
  $destroy() {
    destroy_component(this, 1);
    this.$destroy = noop;
  }
  /**
   * @template {Extract<keyof Events, string>} K
   * @param {K} type
   * @param {((e: Events[K]) => void) | null | undefined} callback
   * @returns {() => void}
   */
  $on(type, callback) {
    if (!is_function(callback)) {
      return noop;
    }
    const callbacks = this.$$.callbacks[type] || (this.$$.callbacks[type] = []);
    callbacks.push(callback);
    return () => {
      const index = callbacks.indexOf(callback);
      if (index !== -1)
        callbacks.splice(index, 1);
    };
  }
  /**
   * @param {Partial<Props>} props
   * @returns {void}
   */
  $set(props) {
    if (this.$$set && !is_empty(props)) {
      this.$$.skip_bound = true;
      this.$$set(props);
      this.$$.skip_bound = false;
    }
  }
}
const PUBLIC_VERSION = "4";
if (typeof window !== "undefined")
  (window.__svelte || (window.__svelte = { v: /* @__PURE__ */ new Set() })).v.add(PUBLIC_VERSION);
const subscriber_queue = [];
function readable(value, start2) {
  return {
    subscribe: writable(value, start2).subscribe
  };
}
function writable(value, start2 = noop) {
  let stop;
  const subscribers = /* @__PURE__ */ new Set();
  function set(new_value) {
    if (safe_not_equal(value, new_value)) {
      value = new_value;
      if (stop) {
        const run_queue = !subscriber_queue.length;
        for (const subscriber of subscribers) {
          subscriber[1]();
          subscriber_queue.push(subscriber, value);
        }
        if (run_queue) {
          for (let i = 0; i < subscriber_queue.length; i += 2) {
            subscriber_queue[i][0](subscriber_queue[i + 1]);
          }
          subscriber_queue.length = 0;
        }
      }
    }
  }
  function update2(fn) {
    set(fn(value));
  }
  function subscribe2(run2, invalidate = noop) {
    const subscriber = [run2, invalidate];
    subscribers.add(subscriber);
    if (subscribers.size === 1) {
      stop = start2(set, update2) || noop;
    }
    run2(value);
    return () => {
      subscribers.delete(subscriber);
      if (subscribers.size === 0 && stop) {
        stop();
        stop = null;
      }
    };
  }
  return { set, update: update2, subscribe: subscribe2 };
}
function unwrap(result) {
  if (result.ok) {
    return result.value;
  } else {
    throw result.error;
  }
}
var Game = /* @__PURE__ */ ((Game2) => {
  Game2[Game2["rs3"] = 0] = "rs3";
  Game2[Game2["osrs"] = 1] = "osrs";
  return Game2;
})(Game || {});
var Client = /* @__PURE__ */ ((Client2) => {
  Client2[Client2["runeLite"] = 0] = "runeLite";
  Client2[Client2["hdos"] = 1] = "hdos";
  Client2[Client2["rs3"] = 2] = "rs3";
  return Client2;
})(Client || {});
const configDefaults = {
  use_dark_theme: true,
  flatpak_rich_presence: false,
  rs_config_uri: "",
  runelite_custom_jar: "",
  runelite_use_custom_jar: false,
  selected_account: "",
  selected_characters: /* @__PURE__ */ new Map(),
  selected_game_accounts: /* @__PURE__ */ new Map(),
  selected_game_index: 1,
  selected_client_index: 1
};
const internalUrl = readable("https://bolt-internal");
const productionClientId = readable(
  "1fddee4e-b100-4f4e-b2b0-097f9088f9d2"
);
const bolt = writable();
const platform = writable("");
const config = writable({ ...configDefaults });
const credentials = writable(/* @__PURE__ */ new Map());
const hasBoltPlugins = writable(false);
const clientListPromise = writable();
const messageList = writable([]);
const pendingOauth = writable({});
const pendingGameAuth = writable([]);
const rs3InstalledHash = writable("");
const runeLiteInstalledId = writable("");
const hdosInstalledVersion = writable("");
const isConfigDirty = writable(false);
const accountList = writable(/* @__PURE__ */ new Map());
const selectedPlay = writable({
  game: Game.osrs,
  client: Client.runeLite
});
const showDisclaimer = writable(false);
function loadTheme() {
  if (configSub.use_dark_theme == false) {
    document.documentElement.classList.remove("dark");
  }
}
function removePendingGameAuth(pending, closeWindow) {
  if (closeWindow) {
    pending.win.close();
  }
  pendingGameAuth.update((data) => {
    data.splice(pendingGameAuthSub.indexOf(pending), 1);
    return data;
  });
}
function loginClicked() {
  if (pendingOauthSub && pendingOauthSub.win && !pendingOauthSub.win.closed) {
    pendingOauthSub.win.focus();
  } else if (pendingOauthSub && pendingOauthSub.win && pendingOauthSub.win.closed || pendingOauthSub) {
    const state = makeRandomState();
    const verifier = makeRandomVerifier();
    makeLoginUrl({
      origin: atob(boltSub.origin),
      redirect: atob(boltSub.redirect),
      authMethod: "",
      loginType: "",
      clientid: atob(boltSub.clientid),
      flow: "launcher",
      pkceState: state,
      pkceCodeVerifier: verifier
    }).then((url) => {
      const win = window.open(url, "", "width=480,height=720");
      pendingOauth.set({ state, verifier, win });
    });
  }
}
function urlSearchParams() {
  const query = new URLSearchParams(window.location.search);
  platform.set(query.get("platform"));
  query.get("flathub") === "1";
  rs3InstalledHash.set(query.get("rs3_linux_installed_hash"));
  runeLiteInstalledId.set(query.get("runelite_installed_id"));
  hdosInstalledVersion.set(query.get("hdos_installed_version"));
  hasBoltPlugins.set(query.get("plugins") === "1");
  const creds = query.get("credentials");
  if (creds) {
    try {
      const credsList = JSON.parse(creds);
      credsList.forEach((value) => {
        credentials.update((data) => {
          data.set(value.sub, value);
          return data;
        });
      });
    } catch (error) {
      err(`Couldn't parse credentials file: ${error}`, false);
    }
  }
  const conf = query.get("config");
  if (conf) {
    try {
      const parsedConf = JSON.parse(conf);
      config.set(parsedConf);
      config.update((data) => {
        if (data.selected_game_accounts) {
          data.selected_characters = new Map(Object.entries(data.selected_game_accounts));
          delete data.selected_game_accounts;
        } else if (data.selected_characters) {
          data.selected_characters = new Map(Object.entries(data.selected_characters));
        }
        return data;
      });
    } catch (error) {
      err(`Couldn't parse config file: ${error}`, false);
    }
  }
}
async function checkRenewCreds(creds, url, clientId) {
  return new Promise((resolve) => {
    if (creds.expiry - Date.now() < 3e4) {
      const postData = new URLSearchParams({
        grant_type: "refresh_token",
        client_id: clientId,
        refresh_token: creds.refresh_token
      });
      const xml = new XMLHttpRequest();
      xml.onreadystatechange = () => {
        if (xml.readyState == 4) {
          if (xml.status == 200) {
            const result = parseCredentials(xml.response);
            const resultCreds = unwrap(result);
            if (resultCreds) {
              creds.access_token = resultCreds.access_token;
              creds.expiry = resultCreds.expiry;
              creds.id_token = resultCreds.id_token;
              creds.login_provider = resultCreds.login_provider;
              creds.refresh_token = resultCreds.refresh_token;
              if (resultCreds.session_id)
                creds.session_id = resultCreds.session_id;
              creds.sub = resultCreds.sub;
              resolve(null);
            } else {
              resolve(0);
            }
          } else {
            resolve(xml.status);
          }
        }
      };
      xml.onerror = () => {
        resolve(0);
      };
      xml.open("POST", url, true);
      xml.setRequestHeader("Content-Type", "application/x-www-form-urlencoded");
      xml.setRequestHeader("Accept", "application/json");
      xml.send(postData);
    } else {
      resolve(null);
    }
  });
}
function parseCredentials(str) {
  const oauthCreds = JSON.parse(str);
  const sections = oauthCreds.id_token.split(".");
  if (sections.length !== 3) {
    const errMsg = `Malformed id_token: ${sections.length} sections, expected 3`;
    err(errMsg, false);
    return { ok: false, error: new Error(errMsg) };
  }
  const header = JSON.parse(atob(sections[0]));
  if (header.typ !== "JWT") {
    const errMsg = `Bad id_token header: typ ${header.typ}, expected JWT`;
    err(errMsg, false);
    return { ok: false, error: new Error(errMsg) };
  }
  const payload = JSON.parse(atob(sections[1]));
  return {
    ok: true,
    value: {
      access_token: oauthCreds.access_token,
      id_token: oauthCreds.id_token,
      refresh_token: oauthCreds.refresh_token,
      sub: payload.sub,
      login_provider: payload.login_provider || null,
      expiry: Date.now() + oauthCreds.expires_in * 1e3,
      session_id: oauthCreds.session_id
    }
  };
}
async function makeLoginUrl(url) {
  const verifierData = new TextEncoder().encode(url.pkceCodeVerifier);
  const digested = await crypto.subtle.digest("SHA-256", verifierData);
  let raw = "";
  const bytes = new Uint8Array(digested);
  for (let i = 0; i < bytes.byteLength; i++) {
    raw += String.fromCharCode(bytes[i]);
  }
  const codeChallenge = btoa(raw).replace(/\+/g, "-").replace(/\//g, "_").replace(/=+$/, "");
  return url.origin.concat("/oauth2/auth?").concat(
    new URLSearchParams({
      auth_method: url.authMethod,
      login_type: url.loginType,
      flow: url.flow,
      response_type: "code",
      client_id: url.clientid,
      redirect_uri: url.redirect,
      code_challenge: codeChallenge,
      code_challenge_method: "S256",
      prompt: "login",
      scope: "openid offline gamesso.token.create user.profile.read",
      state: url.pkceState
    }).toString()
  );
}
function makeRandomVerifier() {
  const t = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-._~";
  const n = new Uint32Array(43);
  crypto.getRandomValues(n);
  return Array.from(n, function(e) {
    return t[e % t.length];
  }).join("");
}
function makeRandomState() {
  const t = 0;
  const r = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
  const n = r.length - 1;
  const o = crypto.getRandomValues(new Uint8Array(12));
  return Array.from(o).map((e) => {
    return Math.round(e * (n - t) / 255 + t);
  }).map((e) => {
    return r[e];
  }).join("");
}
async function handleNewSessionId(creds, accountsUrl, accountsInfoPromise) {
  return new Promise((resolve) => {
    const xml = new XMLHttpRequest();
    xml.onreadystatechange = () => {
      if (xml.readyState == 4) {
        if (xml.status == 200) {
          accountsInfoPromise.then((accountInfo) => {
            if (typeof accountInfo !== "number") {
              const account = {
                id: accountInfo.id,
                userId: accountInfo.userId,
                displayName: accountInfo.displayName,
                suffix: accountInfo.suffix,
                characters: /* @__PURE__ */ new Map()
              };
              msg(`Successfully added login for ${account.displayName}`);
              JSON.parse(xml.response).forEach((acc) => {
                account.characters.set(acc.accountId, {
                  accountId: acc.accountId,
                  displayName: acc.displayName,
                  userHash: acc.userHash
                });
              });
              addNewAccount(account);
              resolve(true);
            } else {
              err(`Error getting account info: ${accountInfo}`, false);
              resolve(false);
            }
          });
        } else {
          err(`Error: from ${accountsUrl}: ${xml.status}: ${xml.response}`, false);
          resolve(false);
        }
      }
    };
    xml.open("GET", accountsUrl, true);
    xml.setRequestHeader("Accept", "application/json");
    xml.setRequestHeader("Authorization", "Bearer ".concat(creds.session_id));
    xml.send();
  });
}
async function handleLogin(win, creds) {
  return await handleStandardLogin(win, creds);
}
async function handleStandardLogin(win, creds) {
  const state = makeRandomState();
  const nonce = crypto.randomUUID();
  const location = atob(boltSub.origin).concat("/oauth2/auth?").concat(
    new URLSearchParams({
      id_token_hint: creds.id_token,
      nonce: btoa(nonce),
      prompt: "consent",
      redirect_uri: "http://localhost",
      response_type: "id_token code",
      state,
      client_id: get_store_value(productionClientId),
      scope: "openid offline"
    }).toString()
  );
  const accountInfoPromise = getStandardAccountInfo(creds);
  if (win) {
    win.location.href = location;
    pendingGameAuth.update((data) => {
      data.push({
        state,
        nonce,
        creds,
        win,
        account_info_promise: accountInfoPromise
      });
      return data;
    });
    return false;
  } else {
    if (!creds.session_id) {
      err("Rejecting stored credentials with missing session_id", false);
      return false;
    }
    return await handleNewSessionId(
      creds,
      atob(boltSub.auth_api).concat("/accounts"),
      accountInfoPromise
    );
  }
}
function getStandardAccountInfo(creds) {
  return new Promise((resolve) => {
    const url = `${atob(boltSub.api)}/users/${creds.sub}/displayName`;
    const xml = new XMLHttpRequest();
    xml.onreadystatechange = () => {
      if (xml.readyState == 4) {
        if (xml.status == 200) {
          resolve(JSON.parse(xml.response));
        } else {
          resolve(xml.status);
        }
      }
    };
    xml.open("GET", url, true);
    xml.setRequestHeader("Authorization", "Bearer ".concat(creds.access_token));
    xml.send();
  });
}
function addNewAccount(account) {
  const updateSelectedPlay = () => {
    selectedPlay.update((data) => {
      data.account = account;
      const [firstKey] = account.characters.keys();
      data.character = account.characters.get(firstKey);
      if (credentialsSub.size > 0)
        data.credentials = credentialsSub.get(account.userId);
      return data;
    });
  };
  accountList.update((data) => {
    data.set(account.userId, account);
    return data;
  });
  if (selectedPlaySub.account && configSub.selected_account) {
    if (account.userId == configSub.selected_account) {
      updateSelectedPlay();
    }
  } else if (!selectedPlaySub.account) {
    updateSelectedPlay();
  }
  pendingOauth.set({});
}
function revokeOauthCreds(accessToken, revokeUrl, clientId) {
  return new Promise((resolve) => {
    const xml = new XMLHttpRequest();
    xml.open("POST", revokeUrl, true);
    xml.onreadystatechange = () => {
      if (xml.readyState == 4) {
        resolve(xml.status);
      }
    };
    xml.setRequestHeader("Content-Type", "application/x-www-form-urlencoded");
    xml.send(new URLSearchParams({ token: accessToken, client_id: clientId }));
  });
}
async function saveAllCreds() {
  const xml = new XMLHttpRequest();
  xml.open("POST", "/save-credentials", true);
  xml.setRequestHeader("Content-Type", "application/json");
  xml.onreadystatechange = () => {
    if (xml.readyState == 4) {
      msg(`Save-credentials status: ${xml.responseText.trim()}`);
    }
  };
  selectedPlay.update((data) => {
    var _a;
    data.credentials = credentialsSub.get((_a = selectedPlaySub.account) == null ? void 0 : _a.userId);
    return data;
  });
  const credsList = [];
  credentialsSub.forEach((value) => {
    credsList.push(value);
  });
  xml.send(JSON.stringify(credsList));
}
function launchRS3Linux(jx_session_id, jx_character_id, jx_display_name) {
  saveConfig();
  const launch = (hash, deb) => {
    const xml2 = new XMLHttpRequest();
    const params = {};
    if (hash)
      params.hash = hash;
    if (jx_session_id)
      params.jx_session_id = jx_session_id;
    if (jx_character_id)
      params.jx_character_id = jx_character_id;
    if (jx_display_name)
      params.jx_display_name = jx_display_name;
    if (configSub.rs_config_uri) {
      params.config_uri = configSub.rs_config_uri;
    } else {
      params.config_uri = atob(boltSub.default_config_uri);
    }
    xml2.open("POST", "/launch-rs3-deb?".concat(new URLSearchParams(params).toString()), true);
    xml2.onreadystatechange = () => {
      if (xml2.readyState == 4) {
        msg(`Game launch status: '${xml2.responseText.trim()}'`);
        if (xml2.status == 200 && hash) {
          rs3InstalledHash.set(hash);
        }
      }
    };
    xml2.send(deb);
  };
  const xml = new XMLHttpRequest();
  const contentUrl = atob(boltSub.content_url);
  const url = contentUrl.concat("dists/trusty/non-free/binary-amd64/Packages");
  xml.open("GET", url, true);
  xml.onreadystatechange = () => {
    if (xml.readyState == 4 && xml.status == 200) {
      const lines = Object.fromEntries(
        xml.response.split("\n").map((x) => {
          return x.split(": ");
        })
      );
      if (!lines.Filename || !lines.Size) {
        err(`Could not parse package data from URL: ${url}`, false);
        launch();
        return;
      }
      if (lines.SHA256 !== get_store_value(rs3InstalledHash)) {
        msg("Downloading RS3 client...");
        const exeXml = new XMLHttpRequest();
        exeXml.open("GET", contentUrl.concat(lines.Filename), true);
        exeXml.responseType = "arraybuffer";
        exeXml.onprogress = (e) => {
          if (e.loaded) {
            messageList.update((data) => {
              data[0].text = `Downloading RS3 client... ${(Math.round(1e3 * e.loaded / e.total) / 10).toFixed(1)}%`;
              return data;
            });
          }
        };
        exeXml.onreadystatechange = () => {
          if (exeXml.readyState == 4 && exeXml.status == 200) {
            launch(lines.SHA256, exeXml.response);
          }
        };
        exeXml.onerror = () => {
          err(`Error downloading game client: from ${url}: non-http error`, false);
          launch();
        };
        exeXml.send();
      } else {
        msg("Latest client is already installed");
        launch();
      }
    }
  };
  xml.onerror = () => {
    err(`Error: from ${url}: non-http error`, false);
    launch();
  };
  xml.send();
}
function launchRuneLiteInner(jx_session_id, jx_character_id, jx_display_name, configure) {
  saveConfig();
  const launchPath = configure ? "/launch-runelite-jar-configure?" : "/launch-runelite-jar?";
  const launch = (id, jar, jarPath) => {
    const xml2 = new XMLHttpRequest();
    const params = {};
    if (id)
      params.id = id;
    if (jarPath)
      params.jar_path = jarPath;
    if (jx_session_id)
      params.jx_session_id = jx_session_id;
    if (jx_character_id)
      params.jx_character_id = jx_character_id;
    if (jx_display_name)
      params.jx_display_name = jx_display_name;
    if (configSub.flatpak_rich_presence)
      params.flatpak_rich_presence = "";
    xml2.open(
      jar ? "POST" : "GET",
      launchPath.concat(new URLSearchParams(params).toString()),
      true
    );
    xml2.onreadystatechange = () => {
      if (xml2.readyState == 4) {
        msg(`Game launch status: '${xml2.responseText.trim()}'`);
        if (xml2.status == 200 && id) {
          runeLiteInstalledId.set(id);
        }
      }
    };
    xml2.send(jar);
  };
  if (configSub.runelite_use_custom_jar) {
    launch(null, null, configSub.runelite_custom_jar);
    return;
  }
  const xml = new XMLHttpRequest();
  const url = "https://api.github.com/repos/runelite/launcher/releases";
  xml.open("GET", url, true);
  xml.onreadystatechange = () => {
    if (xml.readyState == 4) {
      if (xml.status == 200) {
        const runelite = JSON.parse(xml.responseText).map((x) => x.assets).flat().find((x) => x.name.toLowerCase() == "runelite.jar");
        if (runelite.id != get_store_value(runeLiteInstalledId)) {
          msg("Downloading RuneLite...");
          const xmlRl = new XMLHttpRequest();
          xmlRl.open("GET", runelite.browser_download_url, true);
          xmlRl.responseType = "arraybuffer";
          xmlRl.onreadystatechange = () => {
            if (xmlRl.readyState == 4) {
              if (xmlRl.status == 200) {
                launch(runelite.id, xmlRl.response);
              } else {
                err(
                  `Error downloading from ${runelite.url}: ${xmlRl.status}: ${xmlRl.responseText}`,
                  false
                );
              }
            }
          };
          xmlRl.onprogress = (e) => {
            if (e.loaded && e.lengthComputable) {
              messageList.update((data) => {
                data[0].text = `Downloading RuneLite... ${(Math.round(1e3 * e.loaded / e.total) / 10).toFixed(1)}%`;
                return data;
              });
            }
          };
          xmlRl.send();
        } else {
          msg("Latest JAR is already installed");
          launch();
        }
      } else {
        err(`Error from ${url}: ${xml.status}: ${xml.responseText}`, false);
      }
    }
  };
  xml.send();
}
function launchRuneLite(jx_session_id, jx_character_id, jx_display_name) {
  return launchRuneLiteInner(jx_session_id, jx_character_id, jx_display_name, false);
}
function launchRuneLiteConfigure(jx_session_id, jx_character_id, jx_display_name) {
  return launchRuneLiteInner(jx_session_id, jx_character_id, jx_display_name, true);
}
function launchHdos(jx_session_id, jx_character_id, jx_display_name) {
  saveConfig();
  const launch = (version, jar) => {
    const xml2 = new XMLHttpRequest();
    const params = {};
    if (version)
      params.version = version;
    if (jx_session_id)
      params.jx_session_id = jx_session_id;
    if (jx_character_id)
      params.jx_character_id = jx_character_id;
    if (jx_display_name)
      params.jx_display_name = jx_display_name;
    xml2.open("POST", "/launch-hdos-jar?".concat(new URLSearchParams(params).toString()), true);
    xml2.onreadystatechange = () => {
      if (xml2.readyState == 4) {
        msg(`Game launch status: '${xml2.responseText.trim()}'`);
        if (xml2.status == 200 && version) {
          hdosInstalledVersion.set(version);
        }
      }
    };
    xml2.send(jar);
  };
  const xml = new XMLHttpRequest();
  const url = "https://cdn.hdos.dev/client/getdown.txt";
  xml.open("GET", url, true);
  xml.onreadystatechange = () => {
    if (xml.readyState == 4) {
      if (xml.status == 200) {
        const versionRegex = xml.responseText.match(
          /^launcher\.version *= *(.*?)$/m
        );
        if (versionRegex && versionRegex.length >= 2) {
          const latestVersion = versionRegex[1];
          if (latestVersion !== get_store_value(hdosInstalledVersion)) {
            const jarUrl = `https://cdn.hdos.dev/launcher/v${latestVersion}/hdos-launcher.jar`;
            msg("Downloading HDOS...");
            const xmlHdos = new XMLHttpRequest();
            xmlHdos.open("GET", jarUrl, true);
            xmlHdos.responseType = "arraybuffer";
            xmlHdos.onreadystatechange = () => {
              if (xmlHdos.readyState == 4) {
                if (xmlHdos.status == 200) {
                  launch(latestVersion, xmlHdos.response);
                } else {
                  const runelite = JSON.parse(xml.responseText).map((x) => x.assets).flat().find(
                    (x) => x.name.toLowerCase() == "runelite.jar"
                  );
                  err(
                    `Error downloading from ${runelite.url}: ${xmlHdos.status}: ${xmlHdos.responseText}`,
                    false
                  );
                }
              }
            };
            xmlHdos.onprogress = (e) => {
              if (e.loaded && e.lengthComputable) {
                messageList.update((data) => {
                  data[0].text = `Downloading HDOS... ${(Math.round(1e3 * e.loaded / e.total) / 10).toFixed(1)}%`;
                  return data;
                });
              }
            };
            xmlHdos.send();
          } else {
            msg("Latest JAR is already installed");
            launch();
          }
        } else {
          msg("Couldn't parse latest launcher version");
          launch();
        }
      } else {
        err(`Error from ${url}: ${xml.status}: ${xml.responseText}`, false);
      }
    }
  };
  xml.send();
}
let saveConfigInProgress = false;
function saveConfig() {
  var _a;
  if (get_store_value(isConfigDirty) && !saveConfigInProgress) {
    saveConfigInProgress = true;
    const xml = new XMLHttpRequest();
    xml.open("POST", "/save-config", true);
    xml.onreadystatechange = () => {
      if (xml.readyState == 4) {
        msg(`Save config status: '${xml.responseText.trim()}'`);
        if (xml.status == 200) {
          isConfigDirty.set(false);
        }
        saveConfigInProgress = false;
      }
    };
    xml.setRequestHeader("Content-Type", "application/json");
    const characters = {};
    (_a = configSub.selected_characters) == null ? void 0 : _a.forEach((value, key) => {
      characters[key] = value;
    });
    const object = {};
    Object.assign(object, configSub);
    object.selected_characters = characters;
    const json = JSON.stringify(object, null, 4);
    xml.send(json);
  }
}
function getNewClientListPromise() {
  return new Promise((resolve, reject) => {
    const xml = new XMLHttpRequest();
    const url = get_store_value(internalUrl).concat("/list-game-clients");
    xml.open("GET", url, true);
    xml.onreadystatechange = () => {
      if (xml.readyState == 4) {
        if (xml.status == 200 && xml.getResponseHeader("content-type") === "application/json") {
          const dict = JSON.parse(xml.responseText);
          resolve(Object.keys(dict).map((uid) => ({ uid, identity: dict[uid].identity || null })));
        } else {
          reject(`error (${xml.responseText})`);
        }
      }
    };
    xml.send();
  });
}
function get_each_context$3(ctx, list, i) {
  const child_ctx = ctx.slice();
  child_ctx[22] = list[i];
  return child_ctx;
}
function create_each_block$3(ctx) {
  let option;
  let t_value = (
    /*account*/
    ctx[22][1].displayName + ""
  );
  let t;
  let option_data_id_value;
  let option_value_value;
  return {
    c() {
      option = element("option");
      t = text(t_value);
      attr(option, "data-id", option_data_id_value = /*account*/
      ctx[22][1].userId);
      attr(option, "class", "dark:bg-slate-900");
      option.__value = option_value_value = /*account*/
      ctx[22][1].displayName;
      set_input_value(option, option.__value);
    },
    m(target, anchor) {
      insert(target, option, anchor);
      append(option, t);
    },
    p(ctx2, dirty) {
      if (dirty & /*$accountList*/
      4 && t_value !== (t_value = /*account*/
      ctx2[22][1].displayName + ""))
        set_data(t, t_value);
      if (dirty & /*$accountList*/
      4 && option_data_id_value !== (option_data_id_value = /*account*/
      ctx2[22][1].userId)) {
        attr(option, "data-id", option_data_id_value);
      }
      if (dirty & /*$accountList*/
      4 && option_value_value !== (option_value_value = /*account*/
      ctx2[22][1].displayName)) {
        option.__value = option_value_value;
        set_input_value(option, option.__value);
      }
    },
    d(detaching) {
      if (detaching) {
        detach(option);
      }
    }
  };
}
function create_fragment$c(ctx) {
  let div1;
  let select;
  let t0;
  let div0;
  let button0;
  let t2;
  let button1;
  let mounted;
  let dispose;
  let each_value = ensure_array_like(
    /*$accountList*/
    ctx[2]
  );
  let each_blocks = [];
  for (let i = 0; i < each_value.length; i += 1) {
    each_blocks[i] = create_each_block$3(get_each_context$3(ctx, each_value, i));
  }
  return {
    c() {
      div1 = element("div");
      select = element("select");
      for (let i = 0; i < each_blocks.length; i += 1) {
        each_blocks[i].c();
      }
      t0 = space();
      div0 = element("div");
      button0 = element("button");
      button0.textContent = "Log In";
      t2 = space();
      button1 = element("button");
      button1.textContent = "Log Out";
      attr(select, "name", "account_select");
      attr(select, "id", "account_select");
      attr(select, "class", "w-full cursor-pointer rounded-lg border-2 border-inherit bg-inherit p-2 text-center");
      attr(button0, "class", "mx-auto mr-2 rounded-lg bg-blue-500 p-2 font-bold text-black duration-200 hover:opacity-75");
      attr(button1, "class", "mx-auto rounded-lg border-2 border-blue-500 p-2 font-bold duration-200 hover:opacity-75");
      attr(div0, "class", "mt-5 flex");
      attr(div1, "class", "z-10 w-48 rounded-lg border-2 border-slate-300 bg-slate-100 p-3 shadow dark:border-slate-800 dark:bg-slate-900");
      attr(div1, "role", "none");
    },
    m(target, anchor) {
      insert(target, div1, anchor);
      append(div1, select);
      for (let i = 0; i < each_blocks.length; i += 1) {
        if (each_blocks[i]) {
          each_blocks[i].m(select, null);
        }
      }
      ctx[7](select);
      append(div1, t0);
      append(div1, div0);
      append(div0, button0);
      append(div0, t2);
      append(div0, button1);
      if (!mounted) {
        dispose = [
          listen(
            select,
            "change",
            /*change_handler*/
            ctx[8]
          ),
          listen(
            button0,
            "click",
            /*click_handler*/
            ctx[9]
          ),
          listen(
            button1,
            "click",
            /*click_handler_1*/
            ctx[10]
          ),
          listen(
            div1,
            "mouseenter",
            /*mouseenter_handler*/
            ctx[11]
          ),
          listen(
            div1,
            "mouseleave",
            /*mouseleave_handler*/
            ctx[12]
          )
        ];
        mounted = true;
      }
    },
    p(ctx2, [dirty]) {
      if (dirty & /*$accountList*/
      4) {
        each_value = ensure_array_like(
          /*$accountList*/
          ctx2[2]
        );
        let i;
        for (i = 0; i < each_value.length; i += 1) {
          const child_ctx = get_each_context$3(ctx2, each_value, i);
          if (each_blocks[i]) {
            each_blocks[i].p(child_ctx, dirty);
          } else {
            each_blocks[i] = create_each_block$3(child_ctx);
            each_blocks[i].c();
            each_blocks[i].m(select, null);
          }
        }
        for (; i < each_blocks.length; i += 1) {
          each_blocks[i].d(1);
        }
        each_blocks.length = each_value.length;
      }
    },
    i: noop,
    o: noop,
    d(detaching) {
      if (detaching) {
        detach(div1);
      }
      destroy_each(each_blocks, detaching);
      ctx[7](null);
      mounted = false;
      run_all(dispose);
    }
  };
}
function instance$b($$self, $$props, $$invalidate) {
  let $selectedPlay;
  let $credentials;
  let $accountList;
  let $config;
  component_subscribe($$self, selectedPlay, ($$value) => $$invalidate(13, $selectedPlay = $$value));
  component_subscribe($$self, credentials, ($$value) => $$invalidate(14, $credentials = $$value));
  component_subscribe($$self, accountList, ($$value) => $$invalidate(2, $accountList = $$value));
  component_subscribe($$self, config, ($$value) => $$invalidate(15, $config = $$value));
  let { showAccountDropdown } = $$props;
  let { hoverAccountButton } = $$props;
  const sOrigin = atob(boltSub.origin);
  const clientId = atob(boltSub.clientid);
  const exchangeUrl = sOrigin.concat("/oauth2/token");
  const revokeUrl = sOrigin.concat("/oauth2/revoke");
  let mousedOver = false;
  let accountSelect;
  function checkClickOutside(evt) {
    if (hoverAccountButton)
      return;
    if (evt.button === 0 && showAccountDropdown && !mousedOver) {
      $$invalidate(5, showAccountDropdown = false);
    }
  }
  function logoutClicked() {
    var _a;
    if (accountSelect.options.length == 0) {
      msg("Logout unsuccessful: no account selected");
      return;
    }
    let creds = $selectedPlay.credentials;
    if ($selectedPlay.account) {
      $accountList.delete((_a = $selectedPlay.account) == null ? void 0 : _a.userId);
      accountList.set($accountList);
    }
    const index = accountSelect.selectedIndex;
    if (index > 0)
      $$invalidate(1, accountSelect.selectedIndex = index - 1, accountSelect);
    else if (index == 0 && $accountList.size > 0) {
      $$invalidate(1, accountSelect.selectedIndex = index + 1, accountSelect);
    } else {
      delete $selectedPlay.account;
      delete $selectedPlay.character;
      delete $selectedPlay.credentials;
    }
    accountChanged();
    if (!creds)
      return;
    checkRenewCreds(creds, exchangeUrl, clientId).then((x) => {
      if (x === null) {
        revokeOauthCreds(creds.access_token, revokeUrl, clientId).then((res) => {
          if (res === 200) {
            msg("Successful logout");
            removeLogin(creds);
          } else {
            err(`Logout unsuccessful: status ${res}`, false);
          }
        });
      } else if (x === 400 || x === 401) {
        msg("Logout unsuccessful: credentials are invalid, so discarding them anyway");
        if (creds)
          removeLogin(creds);
      } else {
        err("Logout unsuccessful: unable to verify credentials due to a network error", false);
      }
    });
  }
  function removeLogin(creds) {
    $credentials.delete(creds.sub);
    saveAllCreds();
  }
  function accountChanged() {
    var _a;
    isConfigDirty.set(true);
    const key = accountSelect[accountSelect.selectedIndex].getAttribute("data-id");
    set_store_value(selectedPlay, $selectedPlay.account = $accountList.get(key), $selectedPlay);
    set_store_value(config, $config.selected_account = key, $config);
    set_store_value(selectedPlay, $selectedPlay.credentials = $credentials.get((_a = $selectedPlay.account) == null ? void 0 : _a.userId), $selectedPlay);
    if ($selectedPlay.account && $selectedPlay.account.characters) {
      const char_select = document.getElementById("character_select");
      char_select.selectedIndex = 0;
      const [first_key] = $selectedPlay.account.characters.keys();
      set_store_value(selectedPlay, $selectedPlay.character = $selectedPlay.account.characters.get(first_key), $selectedPlay);
    }
  }
  addEventListener("mousedown", checkClickOutside);
  onMount(() => {
    var _a;
    let index = 0;
    $accountList.forEach((value, _key) => {
      var _a2;
      if (value.displayName == ((_a2 = $selectedPlay.account) == null ? void 0 : _a2.displayName)) {
        $$invalidate(1, accountSelect.selectedIndex = index, accountSelect);
      }
      index++;
    });
    set_store_value(selectedPlay, $selectedPlay.credentials = $credentials.get((_a = $selectedPlay.account) == null ? void 0 : _a.userId), $selectedPlay);
  });
  onDestroy(() => {
    removeEventListener("mousedown", checkClickOutside);
  });
  function select_binding($$value) {
    binding_callbacks[$$value ? "unshift" : "push"](() => {
      accountSelect = $$value;
      $$invalidate(1, accountSelect);
    });
  }
  const change_handler = () => {
    accountChanged();
  };
  const click_handler = () => {
    loginClicked();
  };
  const click_handler_1 = () => {
    logoutClicked();
  };
  const mouseenter_handler = () => {
    $$invalidate(0, mousedOver = true);
  };
  const mouseleave_handler = () => {
    $$invalidate(0, mousedOver = false);
  };
  $$self.$$set = ($$props2) => {
    if ("showAccountDropdown" in $$props2)
      $$invalidate(5, showAccountDropdown = $$props2.showAccountDropdown);
    if ("hoverAccountButton" in $$props2)
      $$invalidate(6, hoverAccountButton = $$props2.hoverAccountButton);
  };
  return [
    mousedOver,
    accountSelect,
    $accountList,
    logoutClicked,
    accountChanged,
    showAccountDropdown,
    hoverAccountButton,
    select_binding,
    change_handler,
    click_handler,
    click_handler_1,
    mouseenter_handler,
    mouseleave_handler
  ];
}
class Account extends SvelteComponent {
  constructor(options) {
    super();
    init(this, options, instance$b, create_fragment$c, safe_not_equal, {
      showAccountDropdown: 5,
      hoverAccountButton: 6
    });
  }
}
function create_else_block$4(ctx) {
  let t;
  return {
    c() {
      t = text("Log In");
    },
    m(target, anchor) {
      insert(target, t, anchor);
    },
    p: noop,
    d(detaching) {
      if (detaching) {
        detach(t);
      }
    }
  };
}
function create_if_block_1$3(ctx) {
  var _a;
  let t_value = (
    /*$selectedPlay*/
    ((_a = ctx[6].account) == null ? void 0 : _a.displayName) + ""
  );
  let t;
  return {
    c() {
      t = text(t_value);
    },
    m(target, anchor) {
      insert(target, t, anchor);
    },
    p(ctx2, dirty) {
      var _a2;
      if (dirty & /*$selectedPlay*/
      64 && t_value !== (t_value = /*$selectedPlay*/
      ((_a2 = ctx2[6].account) == null ? void 0 : _a2.displayName) + ""))
        set_data(t, t_value);
    },
    d(detaching) {
      if (detaching) {
        detach(t);
      }
    }
  };
}
function create_if_block$5(ctx) {
  let div;
  let account;
  let updating_showAccountDropdown;
  let current;
  function account_showAccountDropdown_binding(value) {
    ctx[20](value);
  }
  let account_props = {
    hoverAccountButton: (
      /*hoverAccountButton*/
      ctx[2]
    )
  };
  if (
    /*showAccountDropdown*/
    ctx[1] !== void 0
  ) {
    account_props.showAccountDropdown = /*showAccountDropdown*/
    ctx[1];
  }
  account = new Account({ props: account_props });
  binding_callbacks.push(() => bind(account, "showAccountDropdown", account_showAccountDropdown_binding));
  return {
    c() {
      div = element("div");
      create_component(account.$$.fragment);
      attr(div, "class", "absolute right-2 top-[72px]");
    },
    m(target, anchor) {
      insert(target, div, anchor);
      mount_component(account, div, null);
      current = true;
    },
    p(ctx2, dirty) {
      const account_changes = {};
      if (dirty & /*hoverAccountButton*/
      4)
        account_changes.hoverAccountButton = /*hoverAccountButton*/
        ctx2[2];
      if (!updating_showAccountDropdown && dirty & /*showAccountDropdown*/
      2) {
        updating_showAccountDropdown = true;
        account_changes.showAccountDropdown = /*showAccountDropdown*/
        ctx2[1];
        add_flush_callback(() => updating_showAccountDropdown = false);
      }
      account.$set(account_changes);
    },
    i(local) {
      if (current)
        return;
      transition_in(account.$$.fragment, local);
      current = true;
    },
    o(local) {
      transition_out(account.$$.fragment, local);
      current = false;
    },
    d(detaching) {
      if (detaching) {
        detach(div);
      }
      destroy_component(account);
    }
  };
}
function create_fragment$b(ctx) {
  let div2;
  let div0;
  let button0;
  let t1;
  let button1;
  let t3;
  let div1;
  let button2;
  let t4;
  let button3;
  let t5;
  let button4;
  let t6;
  let current;
  let mounted;
  let dispose;
  function select_block_type(ctx2, dirty) {
    if (
      /*$selectedPlay*/
      ctx2[6].account
    )
      return create_if_block_1$3;
    return create_else_block$4;
  }
  let current_block_type = select_block_type(ctx);
  let if_block0 = current_block_type(ctx);
  let if_block1 = (
    /*showAccountDropdown*/
    ctx[1] && create_if_block$5(ctx)
  );
  return {
    c() {
      div2 = element("div");
      div0 = element("div");
      button0 = element("button");
      button0.textContent = "RS3";
      t1 = space();
      button1 = element("button");
      button1.textContent = "OSRS";
      t3 = space();
      div1 = element("div");
      button2 = element("button");
      button2.innerHTML = `<img src="svgs/lightbulb-solid.svg" class="h-6 w-6" alt="Change Theme"/>`;
      t4 = space();
      button3 = element("button");
      button3.innerHTML = `<img src="svgs/gear-solid.svg" class="h-6 w-6" alt="Settings"/>`;
      t5 = space();
      button4 = element("button");
      if_block0.c();
      t6 = space();
      if (if_block1)
        if_block1.c();
      attr(button0, "class", "mx-1 w-20 rounded-lg border-2 border-blue-500 p-2 duration-200 hover:opacity-75");
      attr(button1, "class", "mx-1 w-20 rounded-lg border-2 border-blue-500 bg-blue-500 p-2 text-black duration-200 hover:opacity-75");
      attr(div0, "class", "m-3 ml-9 font-bold");
      attr(button2, "class", "my-3 h-10 w-10 rounded-full bg-blue-500 p-2 duration-200 hover:rotate-45 hover:opacity-75");
      attr(button3, "class", "m-3 h-10 w-10 rounded-full bg-blue-500 p-2 duration-200 hover:rotate-45 hover:opacity-75");
      attr(button4, "class", "m-2 w-48 rounded-lg border-2 border-slate-300 bg-inherit p-2 text-center font-bold text-black duration-200 hover:opacity-75 dark:border-slate-800 dark:text-slate-50");
      attr(div1, "class", "ml-auto flex");
      attr(div2, "class", "fixed top-0 flex h-16 w-screen border-b-2 border-slate-300 bg-slate-100 duration-200 dark:border-slate-800 dark:bg-slate-900");
    },
    m(target, anchor) {
      insert(target, div2, anchor);
      append(div2, div0);
      append(div0, button0);
      ctx[10](button0);
      append(div0, t1);
      append(div0, button1);
      ctx[12](button1);
      append(div2, t3);
      append(div2, div1);
      append(div1, button2);
      append(div1, t4);
      append(div1, button3);
      append(div1, t5);
      append(div1, button4);
      if_block0.m(button4, null);
      ctx[16](button4);
      append(div2, t6);
      if (if_block1)
        if_block1.m(div2, null);
      current = true;
      if (!mounted) {
        dispose = [
          listen(
            button0,
            "click",
            /*click_handler*/
            ctx[11]
          ),
          listen(
            button1,
            "click",
            /*click_handler_1*/
            ctx[13]
          ),
          listen(
            button2,
            "click",
            /*click_handler_2*/
            ctx[14]
          ),
          listen(
            button3,
            "click",
            /*click_handler_3*/
            ctx[15]
          ),
          listen(
            button4,
            "mouseenter",
            /*mouseenter_handler*/
            ctx[17]
          ),
          listen(
            button4,
            "mouseleave",
            /*mouseleave_handler*/
            ctx[18]
          ),
          listen(
            button4,
            "click",
            /*click_handler_4*/
            ctx[19]
          )
        ];
        mounted = true;
      }
    },
    p(ctx2, [dirty]) {
      if (current_block_type === (current_block_type = select_block_type(ctx2)) && if_block0) {
        if_block0.p(ctx2, dirty);
      } else {
        if_block0.d(1);
        if_block0 = current_block_type(ctx2);
        if (if_block0) {
          if_block0.c();
          if_block0.m(button4, null);
        }
      }
      if (
        /*showAccountDropdown*/
        ctx2[1]
      ) {
        if (if_block1) {
          if_block1.p(ctx2, dirty);
          if (dirty & /*showAccountDropdown*/
          2) {
            transition_in(if_block1, 1);
          }
        } else {
          if_block1 = create_if_block$5(ctx2);
          if_block1.c();
          transition_in(if_block1, 1);
          if_block1.m(div2, null);
        }
      } else if (if_block1) {
        group_outros();
        transition_out(if_block1, 1, 1, () => {
          if_block1 = null;
        });
        check_outros();
      }
    },
    i(local) {
      if (current)
        return;
      transition_in(if_block1);
      current = true;
    },
    o(local) {
      transition_out(if_block1);
      current = false;
    },
    d(detaching) {
      if (detaching) {
        detach(div2);
      }
      ctx[10](null);
      ctx[12](null);
      if_block0.d();
      ctx[16](null);
      if (if_block1)
        if_block1.d();
      mounted = false;
      run_all(dispose);
    }
  };
}
function instance$a($$self, $$props, $$invalidate) {
  let $config;
  let $isConfigDirty;
  let $selectedPlay;
  component_subscribe($$self, config, ($$value) => $$invalidate(21, $config = $$value));
  component_subscribe($$self, isConfigDirty, ($$value) => $$invalidate(22, $isConfigDirty = $$value));
  component_subscribe($$self, selectedPlay, ($$value) => $$invalidate(6, $selectedPlay = $$value));
  let { showSettings } = $$props;
  let showAccountDropdown = false;
  let hoverAccountButton = false;
  let rs3Button;
  let osrsButton;
  let accountButton;
  function change_theme() {
    let html = document.documentElement;
    if (html.classList.contains("dark"))
      html.classList.remove("dark");
    else
      html.classList.add("dark");
    set_store_value(config, $config.use_dark_theme = !$config.use_dark_theme, $config);
    set_store_value(isConfigDirty, $isConfigDirty = true, $isConfigDirty);
  }
  function toggle_game(game) {
    switch (game) {
      case Game.osrs:
        set_store_value(selectedPlay, $selectedPlay.game = Game.osrs, $selectedPlay);
        set_store_value(selectedPlay, $selectedPlay.client = $config.selected_client_index, $selectedPlay);
        set_store_value(config, $config.selected_game_index = Game.osrs, $config);
        set_store_value(isConfigDirty, $isConfigDirty = true, $isConfigDirty);
        osrsButton.classList.add("bg-blue-500", "text-black");
        rs3Button.classList.remove("bg-blue-500", "text-black");
        break;
      case Game.rs3:
        set_store_value(selectedPlay, $selectedPlay.game = Game.rs3, $selectedPlay);
        set_store_value(config, $config.selected_game_index = Game.rs3, $config);
        set_store_value(isConfigDirty, $isConfigDirty = true, $isConfigDirty);
        osrsButton.classList.remove("bg-blue-500", "text-black");
        rs3Button.classList.add("bg-blue-500", "text-black");
        break;
    }
  }
  function toggle_account() {
    if (accountButton.innerHTML == "Log In") {
      loginClicked();
      return;
    }
    $$invalidate(1, showAccountDropdown = !showAccountDropdown);
  }
  onMount(() => {
    toggle_game($config.selected_game_index);
  });
  function button0_binding($$value) {
    binding_callbacks[$$value ? "unshift" : "push"](() => {
      rs3Button = $$value;
      $$invalidate(3, rs3Button);
    });
  }
  const click_handler = () => {
    toggle_game(Game.rs3);
  };
  function button1_binding($$value) {
    binding_callbacks[$$value ? "unshift" : "push"](() => {
      osrsButton = $$value;
      $$invalidate(4, osrsButton);
    });
  }
  const click_handler_1 = () => {
    toggle_game(Game.osrs);
  };
  const click_handler_22 = () => change_theme();
  const click_handler_3 = () => {
    $$invalidate(0, showSettings = true);
  };
  function button4_binding($$value) {
    binding_callbacks[$$value ? "unshift" : "push"](() => {
      accountButton = $$value;
      $$invalidate(5, accountButton);
    });
  }
  const mouseenter_handler = () => {
    $$invalidate(2, hoverAccountButton = true);
  };
  const mouseleave_handler = () => {
    $$invalidate(2, hoverAccountButton = false);
  };
  const click_handler_4 = () => toggle_account();
  function account_showAccountDropdown_binding(value) {
    showAccountDropdown = value;
    $$invalidate(1, showAccountDropdown);
  }
  $$self.$$set = ($$props2) => {
    if ("showSettings" in $$props2)
      $$invalidate(0, showSettings = $$props2.showSettings);
  };
  return [
    showSettings,
    showAccountDropdown,
    hoverAccountButton,
    rs3Button,
    osrsButton,
    accountButton,
    $selectedPlay,
    change_theme,
    toggle_game,
    toggle_account,
    button0_binding,
    click_handler,
    button1_binding,
    click_handler_1,
    click_handler_22,
    click_handler_3,
    button4_binding,
    mouseenter_handler,
    mouseleave_handler,
    click_handler_4,
    account_showAccountDropdown_binding
  ];
}
class TopBar extends SvelteComponent {
  constructor(options) {
    super();
    init(this, options, instance$a, create_fragment$b, safe_not_equal, { showSettings: 0 });
  }
}
function create_fragment$a(ctx) {
  let div2;
  let div0;
  let t;
  let div1;
  let mounted;
  let dispose;
  return {
    c() {
      div2 = element("div");
      div0 = element("div");
      div0.innerHTML = ``;
      t = space();
      div1 = element("div");
      div1.innerHTML = ``;
      attr(div0, "class", "absolute left-0 top-0 z-10 h-screen w-screen backdrop-blur-sm backdrop-filter");
      attr(div1, "class", "absolute left-0 top-0 z-10 h-screen w-screen bg-black opacity-75");
      attr(div1, "role", "none");
    },
    m(target, anchor) {
      insert(target, div2, anchor);
      append(div2, div0);
      append(div2, t);
      append(div2, div1);
      if (!mounted) {
        dispose = listen(
          div1,
          "click",
          /*click_handler*/
          ctx[1]
        );
        mounted = true;
      }
    },
    p: noop,
    i: noop,
    o: noop,
    d(detaching) {
      if (detaching) {
        detach(div2);
      }
      mounted = false;
      dispose();
    }
  };
}
function instance$9($$self) {
  const dispatch = createEventDispatcher();
  const click_handler = () => {
    dispatch("click");
  };
  return [dispatch, click_handler];
}
class Backdrop extends SvelteComponent {
  constructor(options) {
    super();
    init(this, options, instance$9, create_fragment$a, safe_not_equal, {});
  }
}
function create_fragment$9(ctx) {
  let div1;
  let backdrop;
  let t0;
  let div0;
  let p0;
  let t2;
  let p1;
  let t4;
  let button;
  let current;
  let mounted;
  let dispose;
  backdrop = new Backdrop({});
  return {
    c() {
      div1 = element("div");
      create_component(backdrop.$$.fragment);
      t0 = space();
      div0 = element("div");
      p0 = element("p");
      p0.textContent = `${/*firstText*/
      ctx[1]}`;
      t2 = space();
      p1 = element("p");
      p1.textContent = `${/*secondText*/
      ctx[2]}`;
      t4 = space();
      button = element("button");
      button.textContent = "I Understand";
      attr(p0, "class", "p-2");
      attr(p1, "class", "p-2");
      attr(button, "class", "m-5 rounded-lg border-2 border-blue-500 p-2 duration-200 hover:opacity-75");
      attr(div0, "class", "absolute left-1/4 top-1/4 z-20 w-1/2 rounded-lg bg-slate-100 p-5 text-center shadow-lg dark:bg-slate-900");
      attr(div1, "id", "disclaimer");
    },
    m(target, anchor) {
      insert(target, div1, anchor);
      mount_component(backdrop, div1, null);
      append(div1, t0);
      append(div1, div0);
      append(div0, p0);
      append(div0, t2);
      append(div0, p1);
      append(div0, t4);
      append(div0, button);
      current = true;
      if (!mounted) {
        dispose = listen(
          button,
          "click",
          /*click_handler*/
          ctx[3]
        );
        mounted = true;
      }
    },
    p: noop,
    i(local) {
      if (current)
        return;
      transition_in(backdrop.$$.fragment, local);
      current = true;
    },
    o(local) {
      transition_out(backdrop.$$.fragment, local);
      current = false;
    },
    d(detaching) {
      if (detaching) {
        detach(div1);
      }
      destroy_component(backdrop);
      mounted = false;
      dispose();
    }
  };
}
function instance$8($$self, $$props, $$invalidate) {
  let $showDisclaimer;
  component_subscribe($$self, showDisclaimer, ($$value) => $$invalidate(0, $showDisclaimer = $$value));
  const firstText = atob("Qm9sdCBpcyBhbiB1bm9mZmljaWFsIHRoaXJkLXBhcnR5IGxhdW5jaGVyLiBJdCdzIGZyZWUgYW5kIG9wZW4tc291cmNlIHNvZnR3YXJlIGxpY2Vuc2VkIHVuZGVyIEFHUEwgMy4wLg==");
  const secondText = atob("SmFnZXggaXMgbm90IHJlc3BvbnNpYmxlIGZvciBhbnkgcHJvYmxlbXMgb3IgZGFtYWdlIGNhdXNlZCBieSB1c2luZyB0aGlzIHByb2R1Y3Qu");
  const click_handler = () => {
    set_store_value(showDisclaimer, $showDisclaimer = false, $showDisclaimer);
  };
  return [$showDisclaimer, firstText, secondText, click_handler];
}
class Disclaimer extends SvelteComponent {
  constructor(options) {
    super();
    init(this, options, instance$8, create_fragment$9, safe_not_equal, {});
  }
}
function create_fragment$8(ctx) {
  let div1;
  let button;
  let mounted;
  let dispose;
  return {
    c() {
      div1 = element("div");
      button = element("button");
      button.innerHTML = `<div class="flex"><img src="svgs/database-solid.svg" alt="Browse app data" class="mr-2 h-7 w-7 rounded-lg bg-violet-500 p-1"/>
			Browse App Data</div>`;
      attr(button, "id", "data_dir_button");
      attr(button, "class", "p-2 hover:opacity-75");
      attr(div1, "id", "general_options");
      attr(div1, "class", "col-span-3 p-5 pt-10");
    },
    m(target, anchor) {
      insert(target, div1, anchor);
      append(div1, button);
      if (!mounted) {
        dispose = listen(
          button,
          "click",
          /*click_handler*/
          ctx[1]
        );
        mounted = true;
      }
    },
    p: noop,
    i: noop,
    o: noop,
    d(detaching) {
      if (detaching) {
        detach(div1);
      }
      mounted = false;
      dispose();
    }
  };
}
function instance$7($$self) {
  function openDataDir() {
    var xml = new XMLHttpRequest();
    xml.open("GET", "/browse-data");
    xml.onreadystatechange = () => {
      if (xml.readyState == 4) {
        msg(`Browse status: '${xml.responseText.trim()}'`);
      }
    };
    xml.send();
  }
  const click_handler = () => {
    openDataDir();
  };
  return [openDataDir, click_handler];
}
class General extends SvelteComponent {
  constructor(options) {
    super();
    init(this, options, instance$7, create_fragment$8, safe_not_equal, {});
  }
}
function create_fragment$7(ctx) {
  let div4;
  let button0;
  let t1;
  let div1;
  let label0;
  let t3;
  let input0;
  let t4;
  let div2;
  let label1;
  let t6;
  let input1;
  let t7;
  let div3;
  let textarea;
  let t8;
  let br;
  let t9;
  let button1;
  let mounted;
  let dispose;
  return {
    c() {
      div4 = element("div");
      button0 = element("button");
      button0.innerHTML = `<div class="flex"><img src="svgs/wrench-solid.svg" alt="Configure RuneLite" class="mr-2 h-7 w-7 rounded-lg bg-pink-500 p-1"/>
			Configure RuneLite</div>`;
      t1 = space();
      div1 = element("div");
      label0 = element("label");
      label0.textContent = "Expose rich presence to Flatpak Discord:";
      t3 = space();
      input0 = element("input");
      t4 = space();
      div2 = element("div");
      label1 = element("label");
      label1.textContent = "Use custom RuneLite JAR:";
      t6 = space();
      input1 = element("input");
      t7 = space();
      div3 = element("div");
      textarea = element("textarea");
      t8 = space();
      br = element("br");
      t9 = space();
      button1 = element("button");
      button1.textContent = "Select File";
      attr(button0, "id", "rl_configure");
      attr(button0, "class", "p-2 pb-5 hover:opacity-75");
      attr(label0, "for", "flatpak_rich_presence");
      attr(input0, "type", "checkbox");
      attr(input0, "name", "flatpak_rich_presence");
      attr(input0, "id", "flatpak_rich_presence");
      attr(input0, "class", "ml-2");
      attr(div1, "id", "flatpak_div");
      attr(div1, "class", "mx-auto border-t-2 border-slate-300 p-2 py-5 dark:border-slate-800");
      attr(label1, "for", "use_custom_jar");
      attr(input1, "type", "checkbox");
      attr(input1, "name", "use_custom_jar");
      attr(input1, "id", "use_custom_jar");
      attr(input1, "class", "ml-2");
      attr(div2, "class", "mx-auto border-t-2 border-slate-300 p-2 pt-5 dark:border-slate-800");
      textarea.disabled = true;
      attr(textarea, "name", "custom_jar_file");
      attr(textarea, "id", "custom_jar_file");
      attr(textarea, "class", "h-10 rounded border-2 border-slate-300 bg-slate-100 text-slate-950 dark:border-slate-800 dark:bg-slate-900 dark:text-slate-50");
      button1.disabled = true;
      attr(button1, "id", "custom_jar_file_button");
      attr(button1, "class", "mt-1 rounded-lg border-2 border-blue-500 p-1 duration-200 hover:opacity-75");
      attr(div3, "id", "custom_jar_div");
      attr(div3, "class", "mx-auto p-2 opacity-25");
      attr(div4, "id", "osrs_options");
      attr(div4, "class", "col-span-3 p-5 pt-10");
    },
    m(target, anchor) {
      insert(target, div4, anchor);
      append(div4, button0);
      append(div4, t1);
      append(div4, div1);
      append(div1, label0);
      append(div1, t3);
      append(div1, input0);
      ctx[12](input0);
      ctx[14](div1);
      append(div4, t4);
      append(div4, div2);
      append(div2, label1);
      append(div2, t6);
      append(div2, input1);
      ctx[15](input1);
      append(div4, t7);
      append(div4, div3);
      append(div3, textarea);
      ctx[17](textarea);
      append(div3, t8);
      append(div3, br);
      append(div3, t9);
      append(div3, button1);
      ctx[19](button1);
      ctx[21](div3);
      if (!mounted) {
        dispose = [
          listen(
            button0,
            "click",
            /*click_handler*/
            ctx[11]
          ),
          listen(
            input0,
            "change",
            /*change_handler*/
            ctx[13]
          ),
          listen(
            input1,
            "change",
            /*change_handler_1*/
            ctx[16]
          ),
          listen(
            textarea,
            "change",
            /*change_handler_2*/
            ctx[18]
          ),
          listen(
            button1,
            "click",
            /*click_handler_1*/
            ctx[20]
          )
        ];
        mounted = true;
      }
    },
    p: noop,
    i: noop,
    o: noop,
    d(detaching) {
      if (detaching) {
        detach(div4);
      }
      ctx[12](null);
      ctx[14](null);
      ctx[15](null);
      ctx[17](null);
      ctx[19](null);
      ctx[21](null);
      mounted = false;
      run_all(dispose);
    }
  };
}
function instance$6($$self, $$props, $$invalidate) {
  let $platform;
  let $config;
  let $selectedPlay;
  let $isConfigDirty;
  component_subscribe($$self, platform, ($$value) => $$invalidate(22, $platform = $$value));
  component_subscribe($$self, config, ($$value) => $$invalidate(23, $config = $$value));
  component_subscribe($$self, selectedPlay, ($$value) => $$invalidate(24, $selectedPlay = $$value));
  component_subscribe($$self, isConfigDirty, ($$value) => $$invalidate(25, $isConfigDirty = $$value));
  let customJarDiv;
  let customJarFile;
  let customJarFileButton;
  let useJar;
  let flatpakPresence;
  let flatpakDiv;
  function toggleJarDiv() {
    customJarDiv.classList.toggle("opacity-25");
    $$invalidate(1, customJarFile.disabled = !customJarFile.disabled, customJarFile);
    $$invalidate(2, customJarFileButton.disabled = !customJarFileButton.disabled, customJarFileButton);
    set_store_value(config, $config.runelite_use_custom_jar = useJar.checked, $config);
    if ($config.runelite_use_custom_jar) {
      if (customJarFile.value)
        set_store_value(config, $config.runelite_custom_jar = customJarFile.value, $config);
    } else {
      $$invalidate(1, customJarFile.value = "", customJarFile);
      set_store_value(config, $config.runelite_custom_jar = "", $config);
      set_store_value(config, $config.runelite_use_custom_jar = false, $config);
    }
    set_store_value(isConfigDirty, $isConfigDirty = true, $isConfigDirty);
  }
  function toggleRichPresence() {
    set_store_value(config, $config.flatpak_rich_presence = flatpakPresence.checked, $config);
    set_store_value(isConfigDirty, $isConfigDirty = true, $isConfigDirty);
  }
  function textChanged() {
    set_store_value(config, $config.runelite_custom_jar = customJarFile.value, $config);
    set_store_value(isConfigDirty, $isConfigDirty = true, $isConfigDirty);
  }
  function selectFile() {
    $$invalidate(3, useJar.disabled = true, useJar);
    $$invalidate(2, customJarFileButton.disabled = true, customJarFileButton);
    var xml = new XMLHttpRequest();
    xml.onreadystatechange = () => {
      if (xml.readyState == 4) {
        if (xml.status == 200) {
          $$invalidate(1, customJarFile.value = xml.responseText, customJarFile);
          set_store_value(config, $config.runelite_custom_jar = xml.responseText, $config);
          set_store_value(isConfigDirty, $isConfigDirty = true, $isConfigDirty);
        }
        $$invalidate(2, customJarFileButton.disabled = false, customJarFileButton);
        $$invalidate(3, useJar.disabled = false, useJar);
      }
    };
    xml.open("GET", "/jar-file-picker", true);
    xml.send();
  }
  function launchConfigure() {
    var _a, _b, _c;
    if (!$selectedPlay.account || !$selectedPlay.character) {
      msg("Please log in to configure RuneLite");
      return;
    }
    launchRuneLiteConfigure((_a = $selectedPlay.credentials) == null ? void 0 : _a.session_id, (_b = $selectedPlay.character) == null ? void 0 : _b.accountId, (_c = $selectedPlay.character) == null ? void 0 : _c.displayName);
  }
  onMount(() => {
    $$invalidate(4, flatpakPresence.checked = $config.flatpak_rich_presence, flatpakPresence);
    $$invalidate(3, useJar.checked = $config.runelite_use_custom_jar, useJar);
    if (useJar.checked && $config.runelite_custom_jar) {
      customJarDiv.classList.remove("opacity-25");
      $$invalidate(1, customJarFile.disabled = false, customJarFile);
      $$invalidate(2, customJarFileButton.disabled = false, customJarFileButton);
      $$invalidate(1, customJarFile.value = $config.runelite_custom_jar, customJarFile);
    } else {
      $$invalidate(3, useJar.checked = false, useJar);
      set_store_value(config, $config.runelite_use_custom_jar = false, $config);
    }
    if ($platform !== "linux") {
      flatpakDiv.remove();
    }
  });
  const click_handler = () => launchConfigure();
  function input0_binding($$value) {
    binding_callbacks[$$value ? "unshift" : "push"](() => {
      flatpakPresence = $$value;
      $$invalidate(4, flatpakPresence);
    });
  }
  const change_handler = () => {
    toggleRichPresence();
  };
  function div1_binding($$value) {
    binding_callbacks[$$value ? "unshift" : "push"](() => {
      flatpakDiv = $$value;
      $$invalidate(5, flatpakDiv);
    });
  }
  function input1_binding($$value) {
    binding_callbacks[$$value ? "unshift" : "push"](() => {
      useJar = $$value;
      $$invalidate(3, useJar);
    });
  }
  const change_handler_1 = () => {
    toggleJarDiv();
  };
  function textarea_binding($$value) {
    binding_callbacks[$$value ? "unshift" : "push"](() => {
      customJarFile = $$value;
      $$invalidate(1, customJarFile);
    });
  }
  const change_handler_2 = () => {
    textChanged();
  };
  function button1_binding($$value) {
    binding_callbacks[$$value ? "unshift" : "push"](() => {
      customJarFileButton = $$value;
      $$invalidate(2, customJarFileButton);
    });
  }
  const click_handler_1 = () => {
    selectFile();
  };
  function div3_binding($$value) {
    binding_callbacks[$$value ? "unshift" : "push"](() => {
      customJarDiv = $$value;
      $$invalidate(0, customJarDiv);
    });
  }
  return [
    customJarDiv,
    customJarFile,
    customJarFileButton,
    useJar,
    flatpakPresence,
    flatpakDiv,
    toggleJarDiv,
    toggleRichPresence,
    textChanged,
    selectFile,
    launchConfigure,
    click_handler,
    input0_binding,
    change_handler,
    div1_binding,
    input1_binding,
    change_handler_1,
    textarea_binding,
    change_handler_2,
    button1_binding,
    click_handler_1,
    div3_binding
  ];
}
class Osrs extends SvelteComponent {
  constructor(options) {
    super();
    init(this, options, instance$6, create_fragment$7, safe_not_equal, {});
  }
}
function create_fragment$6(ctx) {
  let div2;
  let div0;
  let label;
  let t1;
  let input;
  let t2;
  let div1;
  let textarea;
  let mounted;
  let dispose;
  return {
    c() {
      div2 = element("div");
      div0 = element("div");
      label = element("label");
      label.textContent = "Use custom config URI:";
      t1 = space();
      input = element("input");
      t2 = space();
      div1 = element("div");
      textarea = element("textarea");
      attr(label, "for", "use_custom_uri");
      attr(input, "type", "checkbox");
      attr(input, "name", "use_custom_uri");
      attr(input, "id", "use_custom_uri");
      attr(input, "class", "ml-2");
      attr(div0, "class", "mx-auto p-2");
      textarea.disabled = true;
      attr(textarea, "name", "config_uri_address");
      attr(textarea, "id", "config_uri_address");
      attr(textarea, "rows", "4");
      attr(textarea, "class", "rounded border-2 border-slate-300 bg-slate-100 text-slate-950 dark:border-slate-800 dark:bg-slate-900 dark:text-slate-50");
      textarea.value = "		";
      attr(div1, "id", "config_uri_div");
      attr(div1, "class", "mx-auto p-2 opacity-25");
      attr(div2, "id", "rs3_options");
      attr(div2, "class", "col-span-3 p-5 pt-10");
    },
    m(target, anchor) {
      insert(target, div2, anchor);
      append(div2, div0);
      append(div0, label);
      append(div0, t1);
      append(div0, input);
      ctx[5](input);
      append(div2, t2);
      append(div2, div1);
      append(div1, textarea);
      ctx[8](textarea);
      ctx[9](div1);
      if (!mounted) {
        dispose = [
          listen(
            input,
            "change",
            /*change_handler*/
            ctx[6]
          ),
          listen(
            textarea,
            "change",
            /*change_handler_1*/
            ctx[7]
          )
        ];
        mounted = true;
      }
    },
    p: noop,
    i: noop,
    o: noop,
    d(detaching) {
      if (detaching) {
        detach(div2);
      }
      ctx[5](null);
      ctx[8](null);
      ctx[9](null);
      mounted = false;
      run_all(dispose);
    }
  };
}
function instance$5($$self, $$props, $$invalidate) {
  let $bolt;
  let $config;
  let $isConfigDirty;
  component_subscribe($$self, bolt, ($$value) => $$invalidate(10, $bolt = $$value));
  component_subscribe($$self, config, ($$value) => $$invalidate(11, $config = $$value));
  component_subscribe($$self, isConfigDirty, ($$value) => $$invalidate(12, $isConfigDirty = $$value));
  let configUriDiv;
  let configUriAddress;
  let useUri;
  function toggleUriDiv() {
    configUriDiv.classList.toggle("opacity-25");
    $$invalidate(1, configUriAddress.disabled = !configUriAddress.disabled, configUriAddress);
    set_store_value(isConfigDirty, $isConfigDirty = true, $isConfigDirty);
    if (!useUri.checked) {
      $$invalidate(1, configUriAddress.value = atob($bolt.default_config_uri), configUriAddress);
      set_store_value(config, $config.rs_config_uri = "", $config);
    }
  }
  function uriAddressChanged() {
    set_store_value(config, $config.rs_config_uri = configUriAddress.value, $config);
    set_store_value(isConfigDirty, $isConfigDirty = true, $isConfigDirty);
  }
  onMount(() => {
    if ($config.rs_config_uri) {
      $$invalidate(1, configUriAddress.value = $config.rs_config_uri, configUriAddress);
      $$invalidate(2, useUri.checked = true, useUri);
      toggleUriDiv();
    } else {
      $$invalidate(1, configUriAddress.value = atob($bolt.default_config_uri), configUriAddress);
    }
  });
  function input_binding($$value) {
    binding_callbacks[$$value ? "unshift" : "push"](() => {
      useUri = $$value;
      $$invalidate(2, useUri);
    });
  }
  const change_handler = () => {
    toggleUriDiv();
  };
  const change_handler_1 = () => {
    uriAddressChanged();
  };
  function textarea_binding($$value) {
    binding_callbacks[$$value ? "unshift" : "push"](() => {
      configUriAddress = $$value;
      $$invalidate(1, configUriAddress);
    });
  }
  function div1_binding($$value) {
    binding_callbacks[$$value ? "unshift" : "push"](() => {
      configUriDiv = $$value;
      $$invalidate(0, configUriDiv);
    });
  }
  return [
    configUriDiv,
    configUriAddress,
    useUri,
    toggleUriDiv,
    uriAddressChanged,
    input_binding,
    change_handler,
    change_handler_1,
    textarea_binding,
    div1_binding
  ];
}
class Rs3 extends SvelteComponent {
  constructor(options) {
    super();
    init(this, options, instance$5, create_fragment$6, safe_not_equal, {});
  }
}
function create_if_block_2$2(ctx) {
  let rs3;
  let current;
  rs3 = new Rs3({});
  return {
    c() {
      create_component(rs3.$$.fragment);
    },
    m(target, anchor) {
      mount_component(rs3, target, anchor);
      current = true;
    },
    i(local) {
      if (current)
        return;
      transition_in(rs3.$$.fragment, local);
      current = true;
    },
    o(local) {
      transition_out(rs3.$$.fragment, local);
      current = false;
    },
    d(detaching) {
      destroy_component(rs3, detaching);
    }
  };
}
function create_if_block_1$2(ctx) {
  let osrs;
  let current;
  osrs = new Osrs({});
  return {
    c() {
      create_component(osrs.$$.fragment);
    },
    m(target, anchor) {
      mount_component(osrs, target, anchor);
      current = true;
    },
    i(local) {
      if (current)
        return;
      transition_in(osrs.$$.fragment, local);
      current = true;
    },
    o(local) {
      transition_out(osrs.$$.fragment, local);
      current = false;
    },
    d(detaching) {
      destroy_component(osrs, detaching);
    }
  };
}
function create_if_block$4(ctx) {
  let general;
  let current;
  general = new General({});
  return {
    c() {
      create_component(general.$$.fragment);
    },
    m(target, anchor) {
      mount_component(general, target, anchor);
      current = true;
    },
    i(local) {
      if (current)
        return;
      transition_in(general.$$.fragment, local);
      current = true;
    },
    o(local) {
      transition_out(general.$$.fragment, local);
      current = false;
    },
    d(detaching) {
      destroy_component(general, detaching);
    }
  };
}
function create_fragment$5(ctx) {
  let div3;
  let backdrop;
  let t0;
  let div2;
  let button0;
  let t1;
  let div1;
  let div0;
  let button1;
  let t2;
  let br0;
  let t3;
  let button2;
  let t4;
  let br1;
  let t5;
  let button3;
  let t6;
  let t7;
  let current_block_type_index;
  let if_block;
  let current;
  let mounted;
  let dispose;
  backdrop = new Backdrop({});
  backdrop.$on(
    "click",
    /*click_handler*/
    ctx[7]
  );
  const if_block_creators = [create_if_block$4, create_if_block_1$2, create_if_block_2$2];
  const if_blocks = [];
  function select_block_type(ctx2, dirty) {
    if (
      /*showOption*/
      ctx2[1] == /*Options*/
      ctx2[5].general
    )
      return 0;
    if (
      /*showOption*/
      ctx2[1] == /*Options*/
      ctx2[5].osrs
    )
      return 1;
    if (
      /*showOption*/
      ctx2[1] == /*Options*/
      ctx2[5].rs3
    )
      return 2;
    return -1;
  }
  if (~(current_block_type_index = select_block_type(ctx))) {
    if_block = if_blocks[current_block_type_index] = if_block_creators[current_block_type_index](ctx);
  }
  return {
    c() {
      div3 = element("div");
      create_component(backdrop.$$.fragment);
      t0 = space();
      div2 = element("div");
      button0 = element("button");
      button0.innerHTML = `<img src="svgs/xmark-solid.svg" class="h-5 w-5" alt="Close"/>`;
      t1 = space();
      div1 = element("div");
      div0 = element("div");
      button1 = element("button");
      t2 = text("General");
      br0 = element("br");
      t3 = space();
      button2 = element("button");
      t4 = text("OSRS");
      br1 = element("br");
      t5 = space();
      button3 = element("button");
      t6 = text("RS3");
      t7 = space();
      if (if_block)
        if_block.c();
      attr(button0, "class", "absolute right-3 top-3 rounded-full bg-rose-500 p-[2px] shadow-lg duration-200 hover:rotate-90 hover:opacity-75");
      attr(button1, "id", "general_button");
      attr(button1, "class", inactiveClass);
      attr(button2, "id", "osrs_button");
      attr(button2, "class", activeClass);
      attr(button3, "id", "rs3_button");
      attr(button3, "class", inactiveClass);
      attr(div0, "class", "relative h-full border-r-2 border-slate-300 pt-10 dark:border-slate-800");
      attr(div1, "class", "grid h-full grid-cols-4");
      attr(div2, "class", "absolute left-[13%] top-[13%] z-20 h-3/4 w-3/4 rounded-lg bg-slate-100 text-center shadow-lg dark:bg-slate-900");
      attr(div3, "id", "settings");
    },
    m(target, anchor) {
      insert(target, div3, anchor);
      mount_component(backdrop, div3, null);
      append(div3, t0);
      append(div3, div2);
      append(div2, button0);
      append(div2, t1);
      append(div2, div1);
      append(div1, div0);
      append(div0, button1);
      append(button1, t2);
      ctx[9](button1);
      append(div0, br0);
      append(div0, t3);
      append(div0, button2);
      append(button2, t4);
      ctx[11](button2);
      append(div0, br1);
      append(div0, t5);
      append(div0, button3);
      append(button3, t6);
      ctx[13](button3);
      append(div1, t7);
      if (~current_block_type_index) {
        if_blocks[current_block_type_index].m(div1, null);
      }
      current = true;
      if (!mounted) {
        dispose = [
          listen(
            button0,
            "click",
            /*click_handler_1*/
            ctx[8]
          ),
          listen(
            button1,
            "click",
            /*click_handler_2*/
            ctx[10]
          ),
          listen(
            button2,
            "click",
            /*click_handler_3*/
            ctx[12]
          ),
          listen(
            button3,
            "click",
            /*click_handler_4*/
            ctx[14]
          )
        ];
        mounted = true;
      }
    },
    p(ctx2, [dirty]) {
      let previous_block_index = current_block_type_index;
      current_block_type_index = select_block_type(ctx2);
      if (current_block_type_index !== previous_block_index) {
        if (if_block) {
          group_outros();
          transition_out(if_blocks[previous_block_index], 1, 1, () => {
            if_blocks[previous_block_index] = null;
          });
          check_outros();
        }
        if (~current_block_type_index) {
          if_block = if_blocks[current_block_type_index];
          if (!if_block) {
            if_block = if_blocks[current_block_type_index] = if_block_creators[current_block_type_index](ctx2);
            if_block.c();
          }
          transition_in(if_block, 1);
          if_block.m(div1, null);
        } else {
          if_block = null;
        }
      }
    },
    i(local) {
      if (current)
        return;
      transition_in(backdrop.$$.fragment, local);
      transition_in(if_block);
      current = true;
    },
    o(local) {
      transition_out(backdrop.$$.fragment, local);
      transition_out(if_block);
      current = false;
    },
    d(detaching) {
      if (detaching) {
        detach(div3);
      }
      destroy_component(backdrop);
      ctx[9](null);
      ctx[11](null);
      ctx[13](null);
      if (~current_block_type_index) {
        if_blocks[current_block_type_index].d();
      }
      mounted = false;
      run_all(dispose);
    }
  };
}
let activeClass = "border-2 border-blue-500 bg-blue-500 hover:opacity-75 font-bold text-black duration-200 rounded-lg p-1 mx-auto my-1 w-3/4";
let inactiveClass = "border-2 border-blue-500 hover:opacity-75 duration-200 rounded-lg p-1 mx-auto my-1 w-3/4";
function instance$4($$self, $$props, $$invalidate) {
  let { showSettings } = $$props;
  var Options = /* @__PURE__ */ ((Options2) => {
    Options2[Options2["general"] = 0] = "general";
    Options2[Options2["osrs"] = 1] = "osrs";
    Options2[Options2["rs3"] = 2] = "rs3";
    return Options2;
  })(Options || {});
  let showOption = 1;
  let generalButton;
  let osrsButton;
  let rs3Button;
  function toggleOptions(option) {
    switch (option) {
      case 0:
        $$invalidate(1, showOption = 0);
        $$invalidate(
          /* general */
          2,
          generalButton.classList.value = activeClass,
          generalButton
        );
        $$invalidate(3, osrsButton.classList.value = inactiveClass, osrsButton);
        $$invalidate(4, rs3Button.classList.value = inactiveClass, rs3Button);
        break;
      case 1:
        $$invalidate(1, showOption = 1);
        $$invalidate(
          /* osrs */
          2,
          generalButton.classList.value = inactiveClass,
          generalButton
        );
        $$invalidate(3, osrsButton.classList.value = activeClass, osrsButton);
        $$invalidate(4, rs3Button.classList.value = inactiveClass, rs3Button);
        break;
      case 2:
        $$invalidate(1, showOption = 2);
        $$invalidate(
          /* rs3 */
          2,
          generalButton.classList.value = inactiveClass,
          generalButton
        );
        $$invalidate(3, osrsButton.classList.value = inactiveClass, osrsButton);
        $$invalidate(4, rs3Button.classList.value = activeClass, rs3Button);
        break;
    }
  }
  function escapeKeyPressed(evt) {
    if (evt.key === "Escape") {
      $$invalidate(0, showSettings = false);
    }
  }
  addEventListener("keydown", escapeKeyPressed);
  onDestroy(() => {
    removeEventListener("keydown", escapeKeyPressed);
  });
  const click_handler = () => {
    $$invalidate(0, showSettings = false);
  };
  const click_handler_1 = () => {
    $$invalidate(0, showSettings = false);
  };
  function button1_binding($$value) {
    binding_callbacks[$$value ? "unshift" : "push"](() => {
      generalButton = $$value;
      $$invalidate(2, generalButton);
    });
  }
  const click_handler_22 = () => {
    toggleOptions(Options.general);
  };
  function button2_binding($$value) {
    binding_callbacks[$$value ? "unshift" : "push"](() => {
      osrsButton = $$value;
      $$invalidate(3, osrsButton);
    });
  }
  const click_handler_3 = () => {
    toggleOptions(Options.osrs);
  };
  function button3_binding($$value) {
    binding_callbacks[$$value ? "unshift" : "push"](() => {
      rs3Button = $$value;
      $$invalidate(4, rs3Button);
    });
  }
  const click_handler_4 = () => {
    toggleOptions(Options.rs3);
  };
  $$self.$$set = ($$props2) => {
    if ("showSettings" in $$props2)
      $$invalidate(0, showSettings = $$props2.showSettings);
  };
  return [
    showSettings,
    showOption,
    generalButton,
    osrsButton,
    rs3Button,
    Options,
    toggleOptions,
    click_handler,
    click_handler_1,
    button1_binding,
    click_handler_22,
    button2_binding,
    click_handler_3,
    button3_binding,
    click_handler_4
  ];
}
class Settings extends SvelteComponent {
  constructor(options) {
    super();
    init(this, options, instance$4, create_fragment$5, safe_not_equal, { showSettings: 0 });
  }
}
function get_each_context$2(ctx, list, i) {
  const child_ctx = ctx.slice();
  child_ctx[1] = list[i];
  return child_ctx;
}
function create_else_block$3(ctx) {
  var _a;
  let li;
  let t0_value = (
    /*message*/
    ((_a = ctx[1].time) == null ? void 0 : _a.toLocaleTimeString()) + ""
  );
  let t0;
  let t1;
  let t2_value = (
    /*message*/
    ctx[1].text + ""
  );
  let t2;
  let t3;
  return {
    c() {
      li = element("li");
      t0 = text(t0_value);
      t1 = text("\n					- ");
      t2 = text(t2_value);
      t3 = space();
    },
    m(target, anchor) {
      insert(target, li, anchor);
      append(li, t0);
      append(li, t1);
      append(li, t2);
      append(li, t3);
    },
    p(ctx2, dirty) {
      var _a2;
      if (dirty & /*$messageList*/
      1 && t0_value !== (t0_value = /*message*/
      ((_a2 = ctx2[1].time) == null ? void 0 : _a2.toLocaleTimeString()) + ""))
        set_data(t0, t0_value);
      if (dirty & /*$messageList*/
      1 && t2_value !== (t2_value = /*message*/
      ctx2[1].text + ""))
        set_data(t2, t2_value);
    },
    d(detaching) {
      if (detaching) {
        detach(li);
      }
    }
  };
}
function create_if_block$3(ctx) {
  var _a;
  let li;
  let t0_value = (
    /*message*/
    ((_a = ctx[1].time) == null ? void 0 : _a.toLocaleTimeString()) + ""
  );
  let t0;
  let t1;
  let t2_value = (
    /*message*/
    ctx[1].text + ""
  );
  let t2;
  let t3;
  return {
    c() {
      li = element("li");
      t0 = text(t0_value);
      t1 = text("\n					- ");
      t2 = text(t2_value);
      t3 = space();
      attr(li, "class", "text-rose-500");
    },
    m(target, anchor) {
      insert(target, li, anchor);
      append(li, t0);
      append(li, t1);
      append(li, t2);
      append(li, t3);
    },
    p(ctx2, dirty) {
      var _a2;
      if (dirty & /*$messageList*/
      1 && t0_value !== (t0_value = /*message*/
      ((_a2 = ctx2[1].time) == null ? void 0 : _a2.toLocaleTimeString()) + ""))
        set_data(t0, t0_value);
      if (dirty & /*$messageList*/
      1 && t2_value !== (t2_value = /*message*/
      ctx2[1].text + ""))
        set_data(t2, t2_value);
    },
    d(detaching) {
      if (detaching) {
        detach(li);
      }
    }
  };
}
function create_each_block$2(ctx) {
  let if_block_anchor;
  function select_block_type(ctx2, dirty) {
    if (
      /*message*/
      ctx2[1].isError
    )
      return create_if_block$3;
    return create_else_block$3;
  }
  let current_block_type = select_block_type(ctx);
  let if_block = current_block_type(ctx);
  return {
    c() {
      if_block.c();
      if_block_anchor = empty();
    },
    m(target, anchor) {
      if_block.m(target, anchor);
      insert(target, if_block_anchor, anchor);
    },
    p(ctx2, dirty) {
      if (current_block_type === (current_block_type = select_block_type(ctx2)) && if_block) {
        if_block.p(ctx2, dirty);
      } else {
        if_block.d(1);
        if_block = current_block_type(ctx2);
        if (if_block) {
          if_block.c();
          if_block.m(if_block_anchor.parentNode, if_block_anchor);
        }
      }
    },
    d(detaching) {
      if (detaching) {
        detach(if_block_anchor);
      }
      if_block.d(detaching);
    }
  };
}
function create_fragment$4(ctx) {
  let div1;
  let div0;
  let t;
  let ol;
  let each_value = ensure_array_like(
    /*$messageList*/
    ctx[0]
  );
  let each_blocks = [];
  for (let i = 0; i < each_value.length; i += 1) {
    each_blocks[i] = create_each_block$2(get_each_context$2(ctx, each_value, i));
  }
  return {
    c() {
      div1 = element("div");
      div0 = element("div");
      div0.innerHTML = `<img src="svgs/circle-info-solid.svg" alt="Message list icon" class="h-7 w-7 rounded-full bg-blue-500 p-[3px] duration-200"/>`;
      t = space();
      ol = element("ol");
      for (let i = 0; i < each_blocks.length; i += 1) {
        each_blocks[i].c();
      }
      attr(div0, "class", "absolute right-2 top-2");
      attr(ol, "id", "message_list");
      attr(ol, "class", "h-full w-[105%] list-inside list-disc overflow-y-auto pl-5 pt-1 marker:text-blue-500");
      attr(div1, "class", "fixed bottom-0 h-1/4 w-screen border-t-2 border-t-slate-300 bg-slate-100 duration-200 dark:border-t-slate-800 dark:bg-slate-900");
    },
    m(target, anchor) {
      insert(target, div1, anchor);
      append(div1, div0);
      append(div1, t);
      append(div1, ol);
      for (let i = 0; i < each_blocks.length; i += 1) {
        if (each_blocks[i]) {
          each_blocks[i].m(ol, null);
        }
      }
    },
    p(ctx2, [dirty]) {
      if (dirty & /*$messageList*/
      1) {
        each_value = ensure_array_like(
          /*$messageList*/
          ctx2[0]
        );
        let i;
        for (i = 0; i < each_value.length; i += 1) {
          const child_ctx = get_each_context$2(ctx2, each_value, i);
          if (each_blocks[i]) {
            each_blocks[i].p(child_ctx, dirty);
          } else {
            each_blocks[i] = create_each_block$2(child_ctx);
            each_blocks[i].c();
            each_blocks[i].m(ol, null);
          }
        }
        for (; i < each_blocks.length; i += 1) {
          each_blocks[i].d(1);
        }
        each_blocks.length = each_value.length;
      }
    },
    i: noop,
    o: noop,
    d(detaching) {
      if (detaching) {
        detach(div1);
      }
      destroy_each(each_blocks, detaching);
    }
  };
}
function instance$3($$self, $$props, $$invalidate) {
  let $messageList;
  component_subscribe($$self, messageList, ($$value) => $$invalidate(0, $messageList = $$value));
  return [$messageList];
}
class Messages extends SvelteComponent {
  constructor(options) {
    super();
    init(this, options, instance$3, create_fragment$4, safe_not_equal, {});
  }
}
function get_each_context$1(ctx, list, i) {
  const child_ctx = ctx.slice();
  child_ctx[13] = list[i];
  return child_ctx;
}
function create_if_block_3$1(ctx) {
  let button;
  let mounted;
  let dispose;
  return {
    c() {
      button = element("button");
      button.textContent = "Plugin menu";
      button.disabled = !get_store_value(hasBoltPlugins);
      attr(button, "class", "mx-auto mb-2 w-52 rounded-lg p-2 font-bold text-black duration-200 enabled:bg-blue-500 enabled:hover:opacity-75 disabled:bg-gray-500");
    },
    m(target, anchor) {
      insert(target, button, anchor);
      if (!mounted) {
        dispose = listen(
          button,
          "click",
          /*click_handler*/
          ctx[8]
        );
        mounted = true;
      }
    },
    p: noop,
    d(detaching) {
      if (detaching) {
        detach(button);
      }
      mounted = false;
      dispose();
    }
  };
}
function create_if_block_2$1(ctx) {
  let label;
  let t1;
  let br;
  let t2;
  let select;
  let option0;
  let option1;
  let mounted;
  let dispose;
  return {
    c() {
      label = element("label");
      label.textContent = "Game Client";
      t1 = space();
      br = element("br");
      t2 = space();
      select = element("select");
      option0 = element("option");
      option0.textContent = "RuneLite";
      option1 = element("option");
      option1.textContent = "HDOS";
      attr(label, "for", "game_client_select");
      attr(label, "class", "text-sm");
      attr(option0, "data-id", Client.runeLite);
      attr(option0, "class", "dark:bg-slate-900");
      option0.__value = "RuneLite";
      set_input_value(option0, option0.__value);
      attr(option1, "data-id", Client.hdos);
      attr(option1, "class", "dark:bg-slate-900");
      option1.__value = "HDOS";
      set_input_value(option1, option1.__value);
      attr(select, "id", "game_client_select");
      attr(select, "class", "mx-auto w-52 cursor-pointer rounded-lg border-2 border-slate-300 bg-inherit p-2 text-inherit duration-200 hover:opacity-75 dark:border-slate-800");
    },
    m(target, anchor) {
      insert(target, label, anchor);
      insert(target, t1, anchor);
      insert(target, br, anchor);
      insert(target, t2, anchor);
      insert(target, select, anchor);
      append(select, option0);
      append(select, option1);
      ctx[7](select);
      if (!mounted) {
        dispose = listen(
          select,
          "change",
          /*clientChanged*/
          ctx[5]
        );
        mounted = true;
      }
    },
    p: noop,
    d(detaching) {
      if (detaching) {
        detach(label);
        detach(t1);
        detach(br);
        detach(t2);
        detach(select);
      }
      ctx[7](null);
      mounted = false;
      dispose();
    }
  };
}
function create_if_block$2(ctx) {
  let each_1_anchor;
  let each_value = ensure_array_like(
    /*$selectedPlay*/
    ctx[4].account.characters
  );
  let each_blocks = [];
  for (let i = 0; i < each_value.length; i += 1) {
    each_blocks[i] = create_each_block$1(get_each_context$1(ctx, each_value, i));
  }
  return {
    c() {
      for (let i = 0; i < each_blocks.length; i += 1) {
        each_blocks[i].c();
      }
      each_1_anchor = empty();
    },
    m(target, anchor) {
      for (let i = 0; i < each_blocks.length; i += 1) {
        if (each_blocks[i]) {
          each_blocks[i].m(target, anchor);
        }
      }
      insert(target, each_1_anchor, anchor);
    },
    p(ctx2, dirty) {
      if (dirty & /*$selectedPlay*/
      16) {
        each_value = ensure_array_like(
          /*$selectedPlay*/
          ctx2[4].account.characters
        );
        let i;
        for (i = 0; i < each_value.length; i += 1) {
          const child_ctx = get_each_context$1(ctx2, each_value, i);
          if (each_blocks[i]) {
            each_blocks[i].p(child_ctx, dirty);
          } else {
            each_blocks[i] = create_each_block$1(child_ctx);
            each_blocks[i].c();
            each_blocks[i].m(each_1_anchor.parentNode, each_1_anchor);
          }
        }
        for (; i < each_blocks.length; i += 1) {
          each_blocks[i].d(1);
        }
        each_blocks.length = each_value.length;
      }
    },
    d(detaching) {
      if (detaching) {
        detach(each_1_anchor);
      }
      destroy_each(each_blocks, detaching);
    }
  };
}
function create_else_block$2(ctx) {
  let t;
  return {
    c() {
      t = text("New Character");
    },
    m(target, anchor) {
      insert(target, t, anchor);
    },
    p: noop,
    d(detaching) {
      if (detaching) {
        detach(t);
      }
    }
  };
}
function create_if_block_1$1(ctx) {
  let t_value = (
    /*character*/
    ctx[13][1].displayName + ""
  );
  let t;
  return {
    c() {
      t = text(t_value);
    },
    m(target, anchor) {
      insert(target, t, anchor);
    },
    p(ctx2, dirty) {
      if (dirty & /*$selectedPlay*/
      16 && t_value !== (t_value = /*character*/
      ctx2[13][1].displayName + ""))
        set_data(t, t_value);
    },
    d(detaching) {
      if (detaching) {
        detach(t);
      }
    }
  };
}
function create_each_block$1(ctx) {
  let option;
  let t;
  let option_data_id_value;
  let option_value_value;
  function select_block_type_1(ctx2, dirty) {
    if (
      /*character*/
      ctx2[13][1].displayName
    )
      return create_if_block_1$1;
    return create_else_block$2;
  }
  let current_block_type = select_block_type_1(ctx);
  let if_block = current_block_type(ctx);
  return {
    c() {
      option = element("option");
      if_block.c();
      t = space();
      attr(option, "data-id", option_data_id_value = /*character*/
      ctx[13][1].accountId);
      attr(option, "class", "dark:bg-slate-900");
      option.__value = option_value_value = "\n						" + /*character*/
      ctx[13][1].displayName + "\n					";
      set_input_value(option, option.__value);
    },
    m(target, anchor) {
      insert(target, option, anchor);
      if_block.m(option, null);
      append(option, t);
    },
    p(ctx2, dirty) {
      if (current_block_type === (current_block_type = select_block_type_1(ctx2)) && if_block) {
        if_block.p(ctx2, dirty);
      } else {
        if_block.d(1);
        if_block = current_block_type(ctx2);
        if (if_block) {
          if_block.c();
          if_block.m(option, t);
        }
      }
      if (dirty & /*$selectedPlay*/
      16 && option_data_id_value !== (option_data_id_value = /*character*/
      ctx2[13][1].accountId)) {
        attr(option, "data-id", option_data_id_value);
      }
      if (dirty & /*$selectedPlay*/
      16 && option_value_value !== (option_value_value = "\n						" + /*character*/
      ctx2[13][1].displayName + "\n					")) {
        option.__value = option_value_value;
        set_input_value(option, option.__value);
      }
    },
    d(detaching) {
      if (detaching) {
        detach(option);
      }
      if_block.d();
    }
  };
}
function create_fragment$3(ctx) {
  let div2;
  let img;
  let img_src_value;
  let t0;
  let button;
  let t2;
  let div0;
  let t3;
  let div1;
  let label;
  let t5;
  let br;
  let t6;
  let select;
  let mounted;
  let dispose;
  function select_block_type(ctx2, dirty) {
    if (
      /*$selectedPlay*/
      ctx2[4].game == Game.osrs
    )
      return create_if_block_2$1;
    if (
      /*$selectedPlay*/
      ctx2[4].game == Game.rs3
    )
      return create_if_block_3$1;
  }
  let current_block_type = select_block_type(ctx);
  let if_block0 = current_block_type && current_block_type(ctx);
  let if_block1 = (
    /*$selectedPlay*/
    ctx[4].account && create_if_block$2(ctx)
  );
  return {
    c() {
      div2 = element("div");
      img = element("img");
      t0 = space();
      button = element("button");
      button.textContent = "Play";
      t2 = space();
      div0 = element("div");
      if (if_block0)
        if_block0.c();
      t3 = space();
      div1 = element("div");
      label = element("label");
      label.textContent = "Character";
      t5 = space();
      br = element("br");
      t6 = space();
      select = element("select");
      if (if_block1)
        if_block1.c();
      if (!src_url_equal(img.src, img_src_value = "svgs/rocket-solid.svg"))
        attr(img, "src", img_src_value);
      attr(img, "alt", "Launch icon");
      attr(img, "class", "mx-auto mb-5 w-24 rounded-3xl bg-gradient-to-br from-rose-500 to-violet-500 p-5");
      attr(button, "class", "mx-auto mb-2 w-52 rounded-lg bg-emerald-500 p-2 font-bold text-black duration-200 hover:opacity-75");
      attr(div0, "class", "mx-auto my-2");
      attr(label, "for", "character_select");
      attr(label, "class", "text-sm");
      attr(select, "id", "character_select");
      attr(select, "class", "mx-auto w-52 cursor-pointer rounded-lg border-2 border-slate-300 bg-inherit p-2 text-inherit duration-200 hover:opacity-75 dark:border-slate-800");
      attr(div1, "class", "mx-auto my-2");
      attr(div2, "class", "bg-grad flex h-full flex-col border-slate-300 p-5 duration-200 dark:border-slate-800");
    },
    m(target, anchor) {
      insert(target, div2, anchor);
      append(div2, img);
      append(div2, t0);
      append(div2, button);
      append(div2, t2);
      append(div2, div0);
      if (if_block0)
        if_block0.m(div0, null);
      append(div2, t3);
      append(div2, div1);
      append(div1, label);
      append(div1, t5);
      append(div1, br);
      append(div1, t6);
      append(div1, select);
      if (if_block1)
        if_block1.m(select, null);
      ctx[9](select);
      if (!mounted) {
        dispose = [
          listen(
            button,
            "click",
            /*play_clicked*/
            ctx[6]
          ),
          listen(
            select,
            "change",
            /*change_handler*/
            ctx[10]
          )
        ];
        mounted = true;
      }
    },
    p(ctx2, [dirty]) {
      if (current_block_type === (current_block_type = select_block_type(ctx2)) && if_block0) {
        if_block0.p(ctx2, dirty);
      } else {
        if (if_block0)
          if_block0.d(1);
        if_block0 = current_block_type && current_block_type(ctx2);
        if (if_block0) {
          if_block0.c();
          if_block0.m(div0, null);
        }
      }
      if (
        /*$selectedPlay*/
        ctx2[4].account
      ) {
        if (if_block1) {
          if_block1.p(ctx2, dirty);
        } else {
          if_block1 = create_if_block$2(ctx2);
          if_block1.c();
          if_block1.m(select, null);
        }
      } else if (if_block1) {
        if_block1.d(1);
        if_block1 = null;
      }
    },
    i: noop,
    o: noop,
    d(detaching) {
      if (detaching) {
        detach(div2);
      }
      if (if_block0) {
        if_block0.d();
      }
      if (if_block1)
        if_block1.d();
      ctx[9](null);
      mounted = false;
      run_all(dispose);
    }
  };
}
function instance$2($$self, $$props, $$invalidate) {
  let $selectedPlay;
  let $config;
  let $isConfigDirty;
  component_subscribe($$self, selectedPlay, ($$value) => $$invalidate(4, $selectedPlay = $$value));
  component_subscribe($$self, config, ($$value) => $$invalidate(11, $config = $$value));
  component_subscribe($$self, isConfigDirty, ($$value) => $$invalidate(12, $isConfigDirty = $$value));
  let { showPluginMenu = false } = $$props;
  let characterSelect;
  let clientSelect;
  function characterChanged() {
    var _a, _b;
    if (!$selectedPlay.account)
      return;
    const key = characterSelect[characterSelect.selectedIndex].getAttribute("data-id");
    set_store_value(selectedPlay, $selectedPlay.character = $selectedPlay.account.characters.get(key), $selectedPlay);
    if ($selectedPlay.character) {
      (_b = $config.selected_characters) == null ? void 0 : _b.set($selectedPlay.account.userId, (_a = $selectedPlay.character) == null ? void 0 : _a.accountId);
    }
    set_store_value(isConfigDirty, $isConfigDirty = true, $isConfigDirty);
  }
  function clientChanged() {
    if (clientSelect.value == "RuneLite") {
      set_store_value(selectedPlay, $selectedPlay.client = Client.runeLite, $selectedPlay);
      set_store_value(config, $config.selected_client_index = Client.runeLite, $config);
    } else if (clientSelect.value == "HDOS") {
      set_store_value(selectedPlay, $selectedPlay.client = Client.hdos, $selectedPlay);
      set_store_value(config, $config.selected_client_index = Client.hdos, $config);
    }
    set_store_value(isConfigDirty, $isConfigDirty = true, $isConfigDirty);
  }
  function play_clicked() {
    var _a, _b, _c, _d, _e, _f, _g, _h, _i;
    if (!$selectedPlay.account || !$selectedPlay.character) {
      msg("Please log in to launch a client");
      return;
    }
    switch ($selectedPlay.game) {
      case Game.osrs:
        if ($selectedPlay.client == Client.runeLite) {
          launchRuneLite((_a = $selectedPlay.credentials) == null ? void 0 : _a.session_id, (_b = $selectedPlay.character) == null ? void 0 : _b.accountId, (_c = $selectedPlay.character) == null ? void 0 : _c.displayName);
        } else if ($selectedPlay.client == Client.hdos) {
          launchHdos((_d = $selectedPlay.credentials) == null ? void 0 : _d.session_id, (_e = $selectedPlay.character) == null ? void 0 : _e.accountId, (_f = $selectedPlay.character) == null ? void 0 : _f.displayName);
        }
        break;
      case Game.rs3:
        launchRS3Linux((_g = $selectedPlay.credentials) == null ? void 0 : _g.session_id, (_h = $selectedPlay.character) == null ? void 0 : _h.accountId, (_i = $selectedPlay.character) == null ? void 0 : _i.displayName);
        break;
    }
  }
  afterUpdate(() => {
    var _a;
    if ($selectedPlay.game == Game.osrs && $selectedPlay.client) {
      $$invalidate(3, clientSelect.selectedIndex = $selectedPlay.client, clientSelect);
    }
    if ($selectedPlay.account && ((_a = $config.selected_characters) == null ? void 0 : _a.has($selectedPlay.account.userId))) {
      for (let i = 0; i < characterSelect.options.length; i++) {
        if (characterSelect[i].getAttribute("data-id") == $config.selected_characters.get($selectedPlay.account.userId)) {
          $$invalidate(2, characterSelect.selectedIndex = i, characterSelect);
        }
      }
    }
  });
  onMount(() => {
    if ($config.selected_game_index == Game.osrs) {
      $$invalidate(3, clientSelect.selectedIndex = $config.selected_client_index, clientSelect);
      set_store_value(selectedPlay, $selectedPlay.client = clientSelect.selectedIndex, $selectedPlay);
    }
  });
  function select_binding($$value) {
    binding_callbacks[$$value ? "unshift" : "push"](() => {
      clientSelect = $$value;
      $$invalidate(3, clientSelect);
    });
  }
  const click_handler = () => {
    $$invalidate(0, showPluginMenu = get_store_value(hasBoltPlugins) ?? false);
  };
  function select_binding_1($$value) {
    binding_callbacks[$$value ? "unshift" : "push"](() => {
      characterSelect = $$value;
      $$invalidate(2, characterSelect);
    });
  }
  const change_handler = () => characterChanged();
  $$self.$$set = ($$props2) => {
    if ("showPluginMenu" in $$props2)
      $$invalidate(0, showPluginMenu = $$props2.showPluginMenu);
  };
  return [
    showPluginMenu,
    characterChanged,
    characterSelect,
    clientSelect,
    $selectedPlay,
    clientChanged,
    play_clicked,
    select_binding,
    click_handler,
    select_binding_1,
    change_handler
  ];
}
class Launch extends SvelteComponent {
  constructor(options) {
    super();
    init(this, options, instance$2, create_fragment$3, safe_not_equal, { showPluginMenu: 0, characterChanged: 1 });
  }
  get characterChanged() {
    return this.$$.ctx[1];
  }
}
function create_fragment$2(ctx) {
  let div;
  return {
    c() {
      div = element("div");
      div.innerHTML = `<img src="svgs/circle-notch-solid.svg" alt="" class="mx-auto h-1/6 w-1/6 animate-spin"/>`;
      attr(div, "class", "container mx-auto bg-slate-100 p-5 text-center text-slate-900 dark:bg-slate-900 dark:text-slate-50");
    },
    m(target, anchor) {
      insert(target, div, anchor);
    },
    p: noop,
    i: noop,
    o: noop,
    d(detaching) {
      if (detaching) {
        detach(div);
      }
    }
  };
}
class Auth extends SvelteComponent {
  constructor(options) {
    super();
    init(this, options, null, create_fragment$2, safe_not_equal, {});
  }
}
function get_each_context(ctx, list, i) {
  const child_ctx = ctx.slice();
  child_ctx[6] = list[i];
  return child_ctx;
}
function create_catch_block(ctx) {
  let p;
  return {
    c() {
      p = element("p");
      p.textContent = "error";
    },
    m(target, anchor) {
      insert(target, p, anchor);
    },
    p: noop,
    d(detaching) {
      if (detaching) {
        detach(p);
      }
    }
  };
}
function create_then_block(ctx) {
  let if_block_anchor;
  function select_block_type(ctx2, dirty) {
    if (
      /*clients*/
      ctx2[5].length == 0
    )
      return create_if_block$1;
    return create_else_block$1;
  }
  let current_block_type = select_block_type(ctx);
  let if_block = current_block_type(ctx);
  return {
    c() {
      if_block.c();
      if_block_anchor = empty();
    },
    m(target, anchor) {
      if_block.m(target, anchor);
      insert(target, if_block_anchor, anchor);
    },
    p(ctx2, dirty) {
      if (current_block_type === (current_block_type = select_block_type(ctx2)) && if_block) {
        if_block.p(ctx2, dirty);
      } else {
        if_block.d(1);
        if_block = current_block_type(ctx2);
        if (if_block) {
          if_block.c();
          if_block.m(if_block_anchor.parentNode, if_block_anchor);
        }
      }
    },
    d(detaching) {
      if (detaching) {
        detach(if_block_anchor);
      }
      if_block.d(detaching);
    }
  };
}
function create_else_block$1(ctx) {
  let each_1_anchor;
  let each_value = ensure_array_like(
    /*clients*/
    ctx[5]
  );
  let each_blocks = [];
  for (let i = 0; i < each_value.length; i += 1) {
    each_blocks[i] = create_each_block(get_each_context(ctx, each_value, i));
  }
  return {
    c() {
      for (let i = 0; i < each_blocks.length; i += 1) {
        each_blocks[i].c();
      }
      each_1_anchor = empty();
    },
    m(target, anchor) {
      for (let i = 0; i < each_blocks.length; i += 1) {
        if (each_blocks[i]) {
          each_blocks[i].m(target, anchor);
        }
      }
      insert(target, each_1_anchor, anchor);
    },
    p(ctx2, dirty) {
      if (dirty & /*$clientListPromise*/
      2) {
        each_value = ensure_array_like(
          /*clients*/
          ctx2[5]
        );
        let i;
        for (i = 0; i < each_value.length; i += 1) {
          const child_ctx = get_each_context(ctx2, each_value, i);
          if (each_blocks[i]) {
            each_blocks[i].p(child_ctx, dirty);
          } else {
            each_blocks[i] = create_each_block(child_ctx);
            each_blocks[i].c();
            each_blocks[i].m(each_1_anchor.parentNode, each_1_anchor);
          }
        }
        for (; i < each_blocks.length; i += 1) {
          each_blocks[i].d(1);
        }
        each_blocks.length = each_value.length;
      }
    },
    d(detaching) {
      if (detaching) {
        detach(each_1_anchor);
      }
      destroy_each(each_blocks, detaching);
    }
  };
}
function create_if_block$1(ctx) {
  let p;
  return {
    c() {
      p = element("p");
      p.textContent = "(start an RS3 game client with plugin library enabled and it will be listed here.)";
    },
    m(target, anchor) {
      insert(target, p, anchor);
    },
    p: noop,
    d(detaching) {
      if (detaching) {
        detach(p);
      }
    }
  };
}
function create_each_block(ctx) {
  let button;
  let t0_value = (
    /*client*/
    (ctx[6].identity || "(unnamed)") + ""
  );
  let t0;
  let t1;
  let br;
  return {
    c() {
      button = element("button");
      t0 = text(t0_value);
      t1 = space();
      br = element("br");
      attr(button, "class", "m-1 h-[28px] w-[95%] rounded-lg border-2 border-blue-500 duration-200 hover:opacity-75");
    },
    m(target, anchor) {
      insert(target, button, anchor);
      append(button, t0);
      insert(target, t1, anchor);
      insert(target, br, anchor);
    },
    p(ctx2, dirty) {
      if (dirty & /*$clientListPromise*/
      2 && t0_value !== (t0_value = /*client*/
      (ctx2[6].identity || "(unnamed)") + ""))
        set_data(t0, t0_value);
    },
    d(detaching) {
      if (detaching) {
        detach(button);
        detach(t1);
        detach(br);
      }
    }
  };
}
function create_pending_block(ctx) {
  let p;
  return {
    c() {
      p = element("p");
      p.textContent = "loading...";
    },
    m(target, anchor) {
      insert(target, p, anchor);
    },
    p: noop,
    d(detaching) {
      if (detaching) {
        detach(p);
      }
    }
  };
}
function create_fragment$1(ctx) {
  let div3;
  let backdrop;
  let t0;
  let div2;
  let button0;
  let t1;
  let div0;
  let button1;
  let t3;
  let hr;
  let t4;
  let promise;
  let t5;
  let div1;
  let current;
  let mounted;
  let dispose;
  backdrop = new Backdrop({});
  backdrop.$on(
    "click",
    /*click_handler*/
    ctx[2]
  );
  let info = {
    ctx,
    current: null,
    token: null,
    hasCatch: true,
    pending: create_pending_block,
    then: create_then_block,
    catch: create_catch_block,
    value: 5,
    error: 9
  };
  handle_promise(promise = /*$clientListPromise*/
  ctx[1], info);
  return {
    c() {
      div3 = element("div");
      create_component(backdrop.$$.fragment);
      t0 = space();
      div2 = element("div");
      button0 = element("button");
      button0.innerHTML = `<img src="svgs/xmark-solid.svg" class="h-5 w-5" alt="Close"/>`;
      t1 = space();
      div0 = element("div");
      button1 = element("button");
      button1.textContent = "Manage Plugins";
      t3 = space();
      hr = element("hr");
      t4 = space();
      info.block.c();
      t5 = space();
      div1 = element("div");
      div1.innerHTML = `<p>menu</p>`;
      attr(button0, "class", "absolute right-3 top-3 rounded-full bg-rose-500 p-[2px] shadow-lg duration-200 hover:rotate-90 hover:opacity-75");
      attr(button1, "class", "mx-auto mb-2 w-[95%] rounded-lg bg-blue-500 p-2 font-bold text-black duration-200 hover:opacity-75");
      attr(hr, "class", "p-1 dark:border-slate-700");
      attr(div0, "class", "left-0 float-left h-full w-[min(180px,_50%)] overflow-hidden border-r-2 border-slate-300 pt-2 dark:border-slate-800");
      attr(div1, "class", "h-full pt-10");
      attr(div2, "class", "absolute left-[5%] top-[5%] z-20 h-[90%] w-[90%] rounded-lg bg-slate-100 text-center shadow-lg dark:bg-slate-900");
    },
    m(target, anchor) {
      insert(target, div3, anchor);
      mount_component(backdrop, div3, null);
      append(div3, t0);
      append(div3, div2);
      append(div2, button0);
      append(div2, t1);
      append(div2, div0);
      append(div0, button1);
      append(div0, t3);
      append(div0, hr);
      append(div0, t4);
      info.block.m(div0, info.anchor = null);
      info.mount = () => div0;
      info.anchor = null;
      append(div2, t5);
      append(div2, div1);
      current = true;
      if (!mounted) {
        dispose = [
          listen(
            button0,
            "click",
            /*click_handler_1*/
            ctx[3]
          ),
          listen(button1, "click", click_handler_2)
        ];
        mounted = true;
      }
    },
    p(new_ctx, [dirty]) {
      ctx = new_ctx;
      info.ctx = ctx;
      if (dirty & /*$clientListPromise*/
      2 && promise !== (promise = /*$clientListPromise*/
      ctx[1]) && handle_promise(promise, info))
        ;
      else {
        update_await_block_branch(info, ctx, dirty);
      }
    },
    i(local) {
      if (current)
        return;
      transition_in(backdrop.$$.fragment, local);
      current = true;
    },
    o(local) {
      transition_out(backdrop.$$.fragment, local);
      current = false;
    },
    d(detaching) {
      if (detaching) {
        detach(div3);
      }
      destroy_component(backdrop);
      info.block.d();
      info.token = null;
      info = null;
      mounted = false;
      run_all(dispose);
    }
  };
}
const click_handler_2 = () => {
};
function instance$1($$self, $$props, $$invalidate) {
  let $clientListPromise;
  component_subscribe($$self, clientListPromise, ($$value) => $$invalidate(1, $clientListPromise = $$value));
  let { showPluginMenu } = $$props;
  clientListPromise.set(getNewClientListPromise());
  function escapeKeyPressed(evt) {
    if (evt.key === "Escape") {
      $$invalidate(0, showPluginMenu = false);
    }
  }
  addEventListener("keydown", escapeKeyPressed);
  onDestroy(() => {
    removeEventListener("keydown", escapeKeyPressed);
  });
  const click_handler = () => {
    $$invalidate(0, showPluginMenu = false);
  };
  const click_handler_1 = () => {
    $$invalidate(0, showPluginMenu = false);
  };
  $$self.$$set = ($$props2) => {
    if ("showPluginMenu" in $$props2)
      $$invalidate(0, showPluginMenu = $$props2.showPluginMenu);
  };
  return [showPluginMenu, $clientListPromise, click_handler, click_handler_1];
}
class PluginMenu extends SvelteComponent {
  constructor(options) {
    super();
    init(this, options, instance$1, create_fragment$1, safe_not_equal, { showPluginMenu: 0 });
  }
}
function create_else_block(ctx) {
  let t0;
  let t1;
  let t2;
  let topbar;
  let updating_showSettings;
  let t3;
  let div2;
  let div0;
  let t4;
  let launch;
  let updating_showPluginMenu;
  let t5;
  let div1;
  let t6;
  let messages;
  let current;
  let if_block0 = (
    /*showPluginMenu*/
    ctx[1] && create_if_block_3(ctx)
  );
  let if_block1 = (
    /*showSettings*/
    ctx[0] && create_if_block_2(ctx)
  );
  let if_block2 = (
    /*$showDisclaimer*/
    ctx[3] && create_if_block_1()
  );
  function topbar_showSettings_binding(value) {
    ctx[6](value);
  }
  let topbar_props = {};
  if (
    /*showSettings*/
    ctx[0] !== void 0
  ) {
    topbar_props.showSettings = /*showSettings*/
    ctx[0];
  }
  topbar = new TopBar({ props: topbar_props });
  binding_callbacks.push(() => bind(topbar, "showSettings", topbar_showSettings_binding));
  function launch_showPluginMenu_binding(value) {
    ctx[7](value);
  }
  let launch_props = {};
  if (
    /*showPluginMenu*/
    ctx[1] !== void 0
  ) {
    launch_props.showPluginMenu = /*showPluginMenu*/
    ctx[1];
  }
  launch = new Launch({ props: launch_props });
  binding_callbacks.push(() => bind(launch, "showPluginMenu", launch_showPluginMenu_binding));
  messages = new Messages({});
  return {
    c() {
      if (if_block0)
        if_block0.c();
      t0 = space();
      if (if_block1)
        if_block1.c();
      t1 = space();
      if (if_block2)
        if_block2.c();
      t2 = space();
      create_component(topbar.$$.fragment);
      t3 = space();
      div2 = element("div");
      div0 = element("div");
      t4 = space();
      create_component(launch.$$.fragment);
      t5 = space();
      div1 = element("div");
      t6 = space();
      create_component(messages.$$.fragment);
      attr(div2, "class", "mt-16 grid h-full grid-flow-col grid-cols-3");
    },
    m(target, anchor) {
      if (if_block0)
        if_block0.m(target, anchor);
      insert(target, t0, anchor);
      if (if_block1)
        if_block1.m(target, anchor);
      insert(target, t1, anchor);
      if (if_block2)
        if_block2.m(target, anchor);
      insert(target, t2, anchor);
      mount_component(topbar, target, anchor);
      insert(target, t3, anchor);
      insert(target, div2, anchor);
      append(div2, div0);
      append(div2, t4);
      mount_component(launch, div2, null);
      append(div2, t5);
      append(div2, div1);
      insert(target, t6, anchor);
      mount_component(messages, target, anchor);
      current = true;
    },
    p(ctx2, dirty) {
      if (
        /*showPluginMenu*/
        ctx2[1]
      ) {
        if (if_block0) {
          if_block0.p(ctx2, dirty);
          if (dirty & /*showPluginMenu*/
          2) {
            transition_in(if_block0, 1);
          }
        } else {
          if_block0 = create_if_block_3(ctx2);
          if_block0.c();
          transition_in(if_block0, 1);
          if_block0.m(t0.parentNode, t0);
        }
      } else if (if_block0) {
        group_outros();
        transition_out(if_block0, 1, 1, () => {
          if_block0 = null;
        });
        check_outros();
      }
      if (
        /*showSettings*/
        ctx2[0]
      ) {
        if (if_block1) {
          if_block1.p(ctx2, dirty);
          if (dirty & /*showSettings*/
          1) {
            transition_in(if_block1, 1);
          }
        } else {
          if_block1 = create_if_block_2(ctx2);
          if_block1.c();
          transition_in(if_block1, 1);
          if_block1.m(t1.parentNode, t1);
        }
      } else if (if_block1) {
        group_outros();
        transition_out(if_block1, 1, 1, () => {
          if_block1 = null;
        });
        check_outros();
      }
      if (
        /*$showDisclaimer*/
        ctx2[3]
      ) {
        if (if_block2) {
          if (dirty & /*$showDisclaimer*/
          8) {
            transition_in(if_block2, 1);
          }
        } else {
          if_block2 = create_if_block_1();
          if_block2.c();
          transition_in(if_block2, 1);
          if_block2.m(t2.parentNode, t2);
        }
      } else if (if_block2) {
        group_outros();
        transition_out(if_block2, 1, 1, () => {
          if_block2 = null;
        });
        check_outros();
      }
      const topbar_changes = {};
      if (!updating_showSettings && dirty & /*showSettings*/
      1) {
        updating_showSettings = true;
        topbar_changes.showSettings = /*showSettings*/
        ctx2[0];
        add_flush_callback(() => updating_showSettings = false);
      }
      topbar.$set(topbar_changes);
      const launch_changes = {};
      if (!updating_showPluginMenu && dirty & /*showPluginMenu*/
      2) {
        updating_showPluginMenu = true;
        launch_changes.showPluginMenu = /*showPluginMenu*/
        ctx2[1];
        add_flush_callback(() => updating_showPluginMenu = false);
      }
      launch.$set(launch_changes);
    },
    i(local) {
      if (current)
        return;
      transition_in(if_block0);
      transition_in(if_block1);
      transition_in(if_block2);
      transition_in(topbar.$$.fragment, local);
      transition_in(launch.$$.fragment, local);
      transition_in(messages.$$.fragment, local);
      current = true;
    },
    o(local) {
      transition_out(if_block0);
      transition_out(if_block1);
      transition_out(if_block2);
      transition_out(topbar.$$.fragment, local);
      transition_out(launch.$$.fragment, local);
      transition_out(messages.$$.fragment, local);
      current = false;
    },
    d(detaching) {
      if (detaching) {
        detach(t0);
        detach(t1);
        detach(t2);
        detach(t3);
        detach(div2);
        detach(t6);
      }
      if (if_block0)
        if_block0.d(detaching);
      if (if_block1)
        if_block1.d(detaching);
      if (if_block2)
        if_block2.d(detaching);
      destroy_component(topbar, detaching);
      destroy_component(launch);
      destroy_component(messages, detaching);
    }
  };
}
function create_if_block(ctx) {
  let auth;
  let current;
  auth = new Auth({});
  return {
    c() {
      create_component(auth.$$.fragment);
    },
    m(target, anchor) {
      mount_component(auth, target, anchor);
      current = true;
    },
    p: noop,
    i(local) {
      if (current)
        return;
      transition_in(auth.$$.fragment, local);
      current = true;
    },
    o(local) {
      transition_out(auth.$$.fragment, local);
      current = false;
    },
    d(detaching) {
      destroy_component(auth, detaching);
    }
  };
}
function create_if_block_3(ctx) {
  let pluginmenu;
  let updating_showPluginMenu;
  let current;
  function pluginmenu_showPluginMenu_binding(value) {
    ctx[4](value);
  }
  let pluginmenu_props = {};
  if (
    /*showPluginMenu*/
    ctx[1] !== void 0
  ) {
    pluginmenu_props.showPluginMenu = /*showPluginMenu*/
    ctx[1];
  }
  pluginmenu = new PluginMenu({ props: pluginmenu_props });
  binding_callbacks.push(() => bind(pluginmenu, "showPluginMenu", pluginmenu_showPluginMenu_binding));
  return {
    c() {
      create_component(pluginmenu.$$.fragment);
    },
    m(target, anchor) {
      mount_component(pluginmenu, target, anchor);
      current = true;
    },
    p(ctx2, dirty) {
      const pluginmenu_changes = {};
      if (!updating_showPluginMenu && dirty & /*showPluginMenu*/
      2) {
        updating_showPluginMenu = true;
        pluginmenu_changes.showPluginMenu = /*showPluginMenu*/
        ctx2[1];
        add_flush_callback(() => updating_showPluginMenu = false);
      }
      pluginmenu.$set(pluginmenu_changes);
    },
    i(local) {
      if (current)
        return;
      transition_in(pluginmenu.$$.fragment, local);
      current = true;
    },
    o(local) {
      transition_out(pluginmenu.$$.fragment, local);
      current = false;
    },
    d(detaching) {
      destroy_component(pluginmenu, detaching);
    }
  };
}
function create_if_block_2(ctx) {
  let settings;
  let updating_showSettings;
  let current;
  function settings_showSettings_binding(value) {
    ctx[5](value);
  }
  let settings_props = {};
  if (
    /*showSettings*/
    ctx[0] !== void 0
  ) {
    settings_props.showSettings = /*showSettings*/
    ctx[0];
  }
  settings = new Settings({ props: settings_props });
  binding_callbacks.push(() => bind(settings, "showSettings", settings_showSettings_binding));
  return {
    c() {
      create_component(settings.$$.fragment);
    },
    m(target, anchor) {
      mount_component(settings, target, anchor);
      current = true;
    },
    p(ctx2, dirty) {
      const settings_changes = {};
      if (!updating_showSettings && dirty & /*showSettings*/
      1) {
        updating_showSettings = true;
        settings_changes.showSettings = /*showSettings*/
        ctx2[0];
        add_flush_callback(() => updating_showSettings = false);
      }
      settings.$set(settings_changes);
    },
    i(local) {
      if (current)
        return;
      transition_in(settings.$$.fragment, local);
      current = true;
    },
    o(local) {
      transition_out(settings.$$.fragment, local);
      current = false;
    },
    d(detaching) {
      destroy_component(settings, detaching);
    }
  };
}
function create_if_block_1(ctx) {
  let disclaimer;
  let current;
  disclaimer = new Disclaimer({});
  return {
    c() {
      create_component(disclaimer.$$.fragment);
    },
    m(target, anchor) {
      mount_component(disclaimer, target, anchor);
      current = true;
    },
    i(local) {
      if (current)
        return;
      transition_in(disclaimer.$$.fragment, local);
      current = true;
    },
    o(local) {
      transition_out(disclaimer.$$.fragment, local);
      current = false;
    },
    d(detaching) {
      destroy_component(disclaimer, detaching);
    }
  };
}
function create_fragment(ctx) {
  let main;
  let current_block_type_index;
  let if_block;
  let current;
  const if_block_creators = [create_if_block, create_else_block];
  const if_blocks = [];
  function select_block_type(ctx2, dirty) {
    if (
      /*authorizing*/
      ctx2[2]
    )
      return 0;
    return 1;
  }
  current_block_type_index = select_block_type(ctx);
  if_block = if_blocks[current_block_type_index] = if_block_creators[current_block_type_index](ctx);
  return {
    c() {
      main = element("main");
      if_block.c();
      attr(main, "class", "h-full");
    },
    m(target, anchor) {
      insert(target, main, anchor);
      if_blocks[current_block_type_index].m(main, null);
      current = true;
    },
    p(ctx2, [dirty]) {
      let previous_block_index = current_block_type_index;
      current_block_type_index = select_block_type(ctx2);
      if (current_block_type_index === previous_block_index) {
        if_blocks[current_block_type_index].p(ctx2, dirty);
      } else {
        group_outros();
        transition_out(if_blocks[previous_block_index], 1, 1, () => {
          if_blocks[previous_block_index] = null;
        });
        check_outros();
        if_block = if_blocks[current_block_type_index];
        if (!if_block) {
          if_block = if_blocks[current_block_type_index] = if_block_creators[current_block_type_index](ctx2);
          if_block.c();
        } else {
          if_block.p(ctx2, dirty);
        }
        transition_in(if_block, 1);
        if_block.m(main, null);
      }
    },
    i(local) {
      if (current)
        return;
      transition_in(if_block);
      current = true;
    },
    o(local) {
      transition_out(if_block);
      current = false;
    },
    d(detaching) {
      if (detaching) {
        detach(main);
      }
      if_blocks[current_block_type_index].d();
    }
  };
}
function instance($$self, $$props, $$invalidate) {
  let $showDisclaimer;
  component_subscribe($$self, showDisclaimer, ($$value) => $$invalidate(3, $showDisclaimer = $$value));
  let showSettings = false;
  let showPluginMenu = false;
  let authorizing = false;
  urlSearchParams();
  const parentWindow = window.opener || window.parent;
  if (parentWindow) {
    const searchParams = new URLSearchParams(window.location.search);
    if (searchParams.get("id_token")) {
      authorizing = true;
      parentWindow.postMessage(
        {
          type: "gameSessionServerAuth",
          code: searchParams.get("code"),
          id_token: searchParams.get("id_token"),
          state: searchParams.get("state")
        },
        "*"
      );
    } else if (searchParams.get("code")) {
      authorizing = true;
      parentWindow.postMessage(
        {
          type: "authCode",
          code: searchParams.get("code"),
          state: searchParams.get("state")
        },
        "*"
      );
    }
  }
  function pluginmenu_showPluginMenu_binding(value) {
    showPluginMenu = value;
    $$invalidate(1, showPluginMenu);
  }
  function settings_showSettings_binding(value) {
    showSettings = value;
    $$invalidate(0, showSettings);
  }
  function topbar_showSettings_binding(value) {
    showSettings = value;
    $$invalidate(0, showSettings);
  }
  function launch_showPluginMenu_binding(value) {
    showPluginMenu = value;
    $$invalidate(1, showPluginMenu);
  }
  return [
    showSettings,
    showPluginMenu,
    authorizing,
    $showDisclaimer,
    pluginmenu_showPluginMenu_binding,
    settings_showSettings_binding,
    topbar_showSettings_binding,
    launch_showPluginMenu_binding
  ];
}
class App extends SvelteComponent {
  constructor(options) {
    super();
    init(this, options, instance, create_fragment, safe_not_equal, {});
  }
}
new App({
  target: document.getElementById("app")
});
const unsubscribers = [];
const internalUrlSub = get_store_value(internalUrl);
let boltSub;
unsubscribers.push(bolt.subscribe((data) => boltSub = data));
let configSub;
unsubscribers.push(config.subscribe((data) => configSub = data));
unsubscribers.push(platform.subscribe((data) => data));
let credentialsSub;
unsubscribers.push(credentials.subscribe((data) => credentialsSub = data));
unsubscribers.push(hasBoltPlugins.subscribe((data) => data ?? false));
let pendingOauthSub;
unsubscribers.push(pendingOauth.subscribe((data) => pendingOauthSub = data));
let pendingGameAuthSub;
unsubscribers.push(pendingGameAuth.subscribe((data) => pendingGameAuthSub = data));
unsubscribers.push(messageList.subscribe((data) => data));
unsubscribers.push(accountList.subscribe((data) => data));
let selectedPlaySub;
unsubscribers.push(selectedPlay.subscribe((data) => selectedPlaySub = data));
function start() {
  var _a;
  const sOrigin = atob(boltSub.origin);
  const clientId = atob(boltSub.clientid);
  const exchangeUrl = sOrigin.concat("/oauth2/token");
  if (credentialsSub.size == 0) {
    showDisclaimer.set(true);
  }
  loadTheme();
  if (configSub.selected_game_accounts && ((_a = configSub.selected_game_accounts) == null ? void 0 : _a.size) > 0) {
    config.update((data) => {
      var _a2;
      data.selected_characters = data.selected_game_accounts;
      (_a2 = data.selected_game_accounts) == null ? void 0 : _a2.clear();
      return data;
    });
  }
  const allowedOrigins = [internalUrlSub, sOrigin, atob(boltSub.origin_2fa)];
  window.addEventListener("message", (event) => {
    if (!allowedOrigins.includes(event.origin)) {
      msg(`discarding window message from origin ${event.origin}`);
      return;
    }
    let pending = pendingOauthSub;
    const xml = new XMLHttpRequest();
    switch (event.data.type) {
      case "authCode":
        if (pending) {
          pendingOauth.set({});
          const post_data = new URLSearchParams({
            grant_type: "authorization_code",
            client_id: atob(boltSub.clientid),
            code: event.data.code,
            code_verifier: pending.verifier,
            redirect_uri: atob(boltSub.redirect)
          });
          xml.onreadystatechange = () => {
            if (xml.readyState == 4) {
              if (xml.status == 200) {
                const result = parseCredentials(xml.response);
                const creds = unwrap(result);
                if (creds) {
                  handleLogin(pending == null ? void 0 : pending.win, creds).then((x) => {
                    if (x) {
                      credentials.update((data) => {
                        data.set(creds.sub, creds);
                        return data;
                      });
                      saveAllCreds();
                    }
                  });
                } else {
                  err(`Error: invalid credentials received`, false);
                  pending.win.close();
                }
              } else {
                err(
                  `Error: from ${exchangeUrl}: ${xml.status}: ${xml.response}`,
                  false
                );
                pending.win.close();
              }
            }
          };
          xml.open("POST", exchangeUrl, true);
          xml.setRequestHeader("Content-Type", "application/x-www-form-urlencoded");
          xml.setRequestHeader("Accept", "application/json");
          xml.send(post_data);
        }
        break;
      case "externalUrl":
        xml.onreadystatechange = () => {
          if (xml.readyState == 4) {
            msg(`External URL status: '${xml.responseText.trim()}'`);
          }
        };
        xml.open("POST", "/open-external-url", true);
        xml.send(event.data.url);
        break;
      case "gameSessionServerAuth":
        pending = pendingGameAuthSub.find((x) => {
          return event.data.state == x.state;
        });
        if (pending) {
          removePendingGameAuth(pending, true);
          const sections = event.data.id_token.split(".");
          if (sections.length !== 3) {
            err(`Malformed id_token: ${sections.length} sections, expected 3`, false);
            break;
          }
          const header = JSON.parse(atob(sections[0]));
          if (header.typ !== "JWT") {
            err(`Bad id_token header: typ ${header.typ}, expected JWT`, false);
            break;
          }
          const payload = JSON.parse(atob(sections[1]));
          if (atob(payload.nonce) !== pending.nonce) {
            err("Incorrect nonce in id_token", false);
            break;
          }
          const sessionsUrl = atob(boltSub.auth_api).concat("/sessions");
          xml.onreadystatechange = () => {
            if (xml.readyState == 4) {
              if (xml.status == 200) {
                const accountsUrl = atob(boltSub.auth_api).concat("/accounts");
                pending.creds.session_id = JSON.parse(xml.response).sessionId;
                handleNewSessionId(
                  pending.creds,
                  accountsUrl,
                  pending.account_info_promise
                ).then((x) => {
                  if (x) {
                    credentials.update((data) => {
                      var _a2;
                      data.set(
                        (_a2 = pending == null ? void 0 : pending.creds) == null ? void 0 : _a2.sub,
                        pending.creds
                      );
                      return data;
                    });
                    saveAllCreds();
                  }
                });
              } else {
                err(
                  `Error: from ${sessionsUrl}: ${xml.status}: ${xml.response}`,
                  false
                );
              }
            }
          };
          xml.open("POST", sessionsUrl, true);
          xml.setRequestHeader("Content-Type", "application/json");
          xml.setRequestHeader("Accept", "application/json");
          xml.send(`{"idToken": "${event.data.id_token}"}`);
        }
        break;
      case "gameClientListUpdate":
        clientListPromise.set(getNewClientListPromise());
        break;
      default:
        msg("Unknown message type: ".concat(event.data.type));
        break;
    }
  });
  (async () => {
    if (credentialsSub.size > 0) {
      credentialsSub.forEach(async (value) => {
        const result = await checkRenewCreds(value, exchangeUrl, clientId);
        if (result !== null && result !== 0) {
          err(`Discarding expired login for #${value.sub}`, false);
          credentials.update((data) => {
            data.delete(value.sub);
            return data;
          });
          saveAllCreds();
        }
        let checkedCred;
        if (result === null && await handleLogin(null, value)) {
          checkedCred = { creds: value, valid: true };
        } else {
          checkedCred = { creds: value, valid: result === 0 };
        }
        if (checkedCred.valid) {
          const creds = value;
          credentials.update((data) => {
            data.set(creds.sub, creds);
            return data;
          });
          saveAllCreds();
        }
      });
    }
    isConfigDirty.set(false);
  })();
}
function msg(str) {
  console.log(str);
  const message = {
    isError: false,
    text: str,
    time: new Date(Date.now())
  };
  messageList.update((list) => {
    list.unshift(message);
    return list;
  });
}
function err(str, doThrow) {
  const message = {
    isError: true,
    text: str,
    time: new Date(Date.now())
  };
  messageList.update((list) => {
    list.unshift(message);
    return list;
  });
  if (!doThrow) {
    console.error(str);
  } else {
    throw new Error(str);
  }
}
bolt.set(s());
onload = () => start();
onunload = () => {
  for (const i in unsubscribers) {
    delete unsubscribers[i];
  }
  saveConfig();
};
