const internal_url = "https://bolt-internal";
const production_client_id = "1fddee4e-b100-4f4e-b2b0-097f9088f9d2";
var platform = null;
var credentials = [];
var credentialsAreDirty = false;
var pendingOauth = null;
var pendingGameAuth = [];
var rs3LinuxInstalledHash = null;
var runeliteInstalledID = null;
var hdosInstalledVersion = null;
var config = {};
var configIsDirty = false;
var isLoading = true;
var isFlathub = null;

// globally-available root elements
var messages = document.getElementById("message_list");
var accountSelect = document.getElementById("account_select");
var loginButton = document.getElementById("login_button");
var logoutButton = document.getElementById("logout_button");
var dataDirButton = document.getElementById("data_dir_button");
var gameAccountSelection = document.getElementById("game_account_select");

// config setting elements
var runeliteFlatpakRichPresence = document.getElementById("flatpak_rich_presence");
var runeliteUseCustomJar = document.getElementById("custom_jar");
var runeliteCustomJar = document.getElementById("custom_jar_file");
var runeliteCustomJarDiv = document.getElementById("custom_jar_div");
var rsConfigUri = document.getElementById("config_uri");
var rsConfigUriDiv = document.getElementById("config_uri_div");

// Checks if `credentials` are about to expire or have already expired,
// and renews them using the oauth endpoint if so.
// Does not save credentials but sets credentialsAreDirty as appropriate.
// Returns null on success or an http status code on failure
function checkRenewCreds(creds, url, client_id) {
    return new Promise((resolve, reject) => {
        // only renew if less than 30 seconds left
        if (creds.expiry - Date.now() < 30000) {
            const post_data = new URLSearchParams({
                grant_type: "refresh_token",
                client_id: client_id,
                refresh_token: creds.refresh_token
            });
            var xml = new XMLHttpRequest();
            xml.onreadystatechange = () => {
                if (xml.readyState == 4) {
                    if (xml.status == 200) {
                        const c = parseCredentials(xml.response);
                        if (c) {
                            Object.assign(creds, c);
                            credentialsAreDirty = true;
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
            xml.open('POST', url, true);
            xml.setRequestHeader("Content-Type", "application/x-www-form-urlencoded");
            xml.setRequestHeader("Accept", "application/json");
            xml.send(post_data);
        } else {
            resolve(null);
        }
    });
}

// parses a response from the oauth endpoint, returning an object to be stored in the `credentials` var
// or returns null on failure
function parseCredentials(str) {
    const oauth_creds = JSON.parse(str);
    const sections = oauth_creds.id_token.split('.');
    if (sections.length !== 3) {
        err(`Malformed id_token: ${sections.length} sections, expected 3`, false);
        return null;
    }
    const header = JSON.parse(atob(sections[0]));
    if (header.typ !== "JWT") {
        err(`Bad id_token header: typ ${header.typ}, expected JWT`, false);
        return null;
    }
    const payload = JSON.parse(atob(sections[1]));
    return {
        access_token: oauth_creds.access_token,
        id_token: oauth_creds.id_token,
        refresh_token: oauth_creds.refresh_token,
        sub: payload.sub,
        login_provider: payload.login_provider || null,
        expiry: Date.now() + (oauth_creds.expires_in * 1000)
    };
}

// builds the url to be opened in the login window
// async because crypto.subtle.digest is async for some reason, so remember to `await`
async function makeLoginUrl(e) {
    const verifier_data = new TextEncoder().encode(e.pkceCodeVerifier);
    const digested = await crypto.subtle.digest("SHA-256", verifier_data);
    var raw = "";
    var bytes = new Uint8Array(digested);
    for (var i = 0; i < bytes.byteLength; i++) {
        raw += String.fromCharCode(bytes[i]);
    }
    const code_challenge = btoa(raw).replace(/\+/g, "-").replace(/\//g, "_").replace(/=+$/, "");
    return e.origin.concat("/oauth2/auth?").concat(new URLSearchParams({
        auth_method: e.authMethod,
        login_type: e.loginType,
        flow: e.flow,
        response_type: "code",
        client_id: e.clientid,
        redirect_uri: e.redirect,
        code_challenge: code_challenge,
        code_challenge_method: "S256",
        prompt: "login",
        scope: "openid offline gamesso.token.create user.profile.read",
        state: e.pkceState
    }));
}

// builds a random PKCE verifier string using crypto.getRandomValues
function makeRandomVerifier() {
    var t = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-._~";
    var n = new Uint32Array(43);
    crypto.getRandomValues(n);
    return Array.from(n, function (e) {
        return t[e % t.length]
    }).join("")
}

// builds a random PKCE state string using crypto.getRandomValues
function makeRandomState() {
    var t = 0;
    var r = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
    var n = r.length - 1;

    var o = crypto.getRandomValues(new Uint8Array(12));
    return Array.from(o).map((e) => {
        return Math.round(e * (n - t) / 255 + t)
    }).map((e) => { return r[e]; }).join("")
}

// remove an entry by its reference from the pendingGameAuth list
// this function assumes this entry is in the list
function removePendingGameAuth(pending, close_window) {
    if (close_window) {
        pending.win.close();
    }
    pendingGameAuth.splice(pendingGameAuth.indexOf(pending), 1);
}

// elements that are commonly changed within the settings menu by the functions below
let general_button = document.getElementById("general_button");
let osrs_button = document.getElementById("osrs_button");
let rs3_button = document.getElementById("rs3_button");
let general_options = document.getElementById("general_options");
let osrs_options = document.getElementById("osrs_options");
let rs3_options = document.getElementById("rs3_options");

let switch_general_options = () => {
    general_options.hidden = false;
    osrs_options.hidden = true;
    rs3_options.hidden = true;

    general_button.classList.add("bg-sky-500");
    osrs_button.classList.remove("bg-sky-500");
    rs3_button.classList.remove("bg-sky-500");
};

let switch_osrs_options = () => {
    general_options.hidden = true;
    osrs_options.hidden = false;
    rs3_options.hidden = true;

    general_button.classList.remove("bg-sky-500");
    osrs_button.classList.add("bg-sky-500");
    rs3_button.classList.remove("bg-sky-500");
};

let switch_rs3_options = () => {
    general_options.hidden = true;
    osrs_options.hidden = true;
    rs3_options.hidden = false;

    general_button.classList.remove("bg-sky-500");
    osrs_button.classList.remove("bg-sky-500");
    rs3_button.classList.add("bg-sky-500");
};

// toggles disclaimer on/off, can be closed by clicking 'I Understand'
let toggle_disclaimer = () => {
    let disclaimer = document.getElementById('disclaimer');
    disclaimer.hidden = !disclaimer.hidden;
};

// after config is loaded, check which theme (light/dark) the user prefers
let load_theme = () => {
    if (config.use_dark_theme == false) {
        document.documentElement.classList.remove('dark');
    }
}

// tailwind can easily change theme by adding or removing 'dark' to the root 'html' element
let change_theme = () => {
    let html = document.documentElement;
    if (html.classList.contains('dark')) html.classList.remove('dark');
    else html.classList.add('dark');

    config.use_dark_theme = html.classList.contains("dark");
    configIsDirty = true;
};

// toggles settings popup
let toggle_settings = () => {
    let settings = document.getElementById("settings");
    settings.hidden = !settings.hidden;
};

// body's onload function
function start(s) {
    const s_origin = atob(s.origin);
    const client_id = atob(s.clientid);
    const exchange_url = s_origin.concat("/oauth2/token");
    const revoke_url = s_origin.concat("/oauth2/revoke");

    const query = new URLSearchParams(window.location.search);
    platform = query.get("platform");
    isFlathub = query.get("flathub") != "0";
    rs3LinuxInstalledHash = query.get("rs3_linux_installed_hash");
    runeliteInstalledID = query.get("runelite_installed_id");
    hdosInstalledVersion = query.get("hdos_installed_version");
    const creds = query.get("credentials");
    if (creds) {
        try {
            // no need to set credentialsAreDirty here because the contents came directly from the file
            credentials = JSON.parse(creds);
        } catch (e) {
            err(`Couldn't parse credentials file: ${e.toString()}`);
        }
    }
    const conf = query.get("config");
    if (conf) {
        try {
            // as above, no need to set configIsDirty
            config = JSON.parse(conf);
        } catch (e) {
            err(`Couldn't parse config file: ${e.toString()}`);
        }
    }

    load_theme();

    const allowed_origins = [internal_url, s_origin, atob(s.origin_2fa)];
    window.addEventListener("message", (event) => {
        if (!allowed_origins.includes(event.origin)) {
            msg(`discarding window message from origin ${event.origin}`);
            return;
        }
        switch (event.data.type) {
            case "authCode":
                var pending = pendingOauth;
                if (pending) {
                    pendingOauth = null;
                    const post_data = new URLSearchParams({
                        grant_type: "authorization_code",
                        client_id: client_id,
                        code: event.data.code,
                        code_verifier: pending.verifier,
                        redirect_uri: atob(s.redirect),
                    });
                    var xml = new XMLHttpRequest();
                    xml.onreadystatechange = () => {
                        if (xml.readyState == 4) {
                            if (xml.status == 200) {
                                const creds = parseCredentials(xml.response);
                                if (creds) {
                                    handleLogin(s, pending.win, creds, exchange_url, client_id).then((x) => {
                                        if (x) {
                                            credentials.push(creds);
                                            credentialsAreDirty = true;
                                            saveAllCreds();
                                        }
                                    });
                                } else {
                                    err(`Error: invalid credentials received`, false);
                                    pending.win.close();
                                }
                            } else {
                                err(`Error: from ${exchange_url}: ${xml.status}: ${xml.response}`, false);
                                pending.win.close();
                            }
                        }
                    };
                    xml.open('POST', exchange_url, true);
                    xml.setRequestHeader("Content-Type", "application/x-www-form-urlencoded");
                    xml.setRequestHeader("Accept", "application/json");
                    xml.send(post_data);
                }
                break;
            case "initAuth":
                msg(`message: init auth: ${event.data.auth_method}`);
                break;
            case "externalUrl":
                var xml = new XMLHttpRequest();
                xml.onreadystatechange = () => {
                    if (xml.readyState == 4) {
                        msg(`External URL status: '${xml.responseText.trim()}'`);
                    }
                };
                xml.open('POST', '/open-external-url', true);
                xml.send(event.data.url);
                break;
            case "gameSessionServerAuth":
                var pending = pendingGameAuth.find((x) => { return event.data.state == x.state });
                if (pending) {
                    removePendingGameAuth(pending, true);
                    const sections = event.data.id_token.split('.');
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
                    const sessions_url = atob(s.auth_api).concat("/sessions");
                    var xml = new XMLHttpRequest();
                    xml.onreadystatechange = () => {
                        if (xml.readyState == 4) {
                            if (xml.status == 200) {
                                const accounts_url = atob(s.auth_api).concat("/accounts");
                                pending.creds.session_id = JSON.parse(xml.response).sessionId;
                                handleNewSessionId(s, pending.creds, accounts_url, pending.account_info_promise).then((x) => {
                                    if (x) {
                                        credentials.push(pending.creds);
                                        credentialsAreDirty = true;
                                        saveAllCreds();
                                    }
                                });
                            } else {
                                err(`Error: from ${sessions_url}: ${xml.status}: ${xml.response}`, false);
                            }
                        }
                    };
                    xml.open('POST', sessions_url, true);
                    xml.setRequestHeader("Content-Type", "application/json");
                    xml.setRequestHeader("Accept", "application/json");
                    xml.send(`{"idToken": "${event.data.id_token}"}`);
                }
                break;
            default:
                msg("Unknown message type: ".concat(event.data.type));
                break;
        }
    });

    rsConfigUri.onchange = () => {
        config.rs_config_uri = rsConfigUri.value;
        configIsDirty = true;
    };

    var rsConfigUriReset = document.getElementById("custom_uri");
    rsConfigUriReset.onchange = () => {
        if (!rsConfigUriReset.checked) {
            rsConfigUri.value = atob(s.default_config_uri);
            delete config.rs_config_uri;
            configIsDirty = true;
            rsConfigUriDiv.classList.add("opacity-25");
            rsConfigUri.disabled = true;
        } else {
            rsConfigUriDiv.classList.remove("opacity-25");
            rsConfigUri.disabled = false;
        }
    };

    if (config.rs_config_uri) {
        rsConfigUri.value = config.rs_config_uri;
        rsConfigUriReset.checked = true;
    } else {
        rsConfigUri.value = atob(s.default_config_uri);
    }
    rsConfigUriReset.onchange();

    runeliteFlatpakRichPresence.checked = config.flatpak_rich_presence || false;
    runeliteFlatpakRichPresence.onchange = () => {
        config.flatpak_rich_presence = runeliteFlatpakRichPresence.checked;
        configIsDirty = true;
    };

    var runeliteCustomJarSelect = document.getElementById("custom_jar_file_button");
    runeliteCustomJarSelect.onclick = () => {
        runeliteUseCustomJar.disabled = true;
        runeliteCustomJarSelect.disabled = true;

        // note: this would give only the file contents, whereas we need the path and don't want the contents:
        //window.showOpenFilePicker({"types": [{"description": "Java Archive (JAR)", "accept": {"application/java-archive": [".jar"]}}]}).then((x) => { });

        var xml = new XMLHttpRequest();
        xml.onreadystatechange = () => {
            if (xml.readyState == 4) {
                // if the user closes the file picker without selecting a file, status here is 204
                if (xml.status == 200) {
                    runeliteCustomJar.value = xml.responseText;
                    config.runelite_custom_jar = xml.responseText;
                    configIsDirty = true;
                }
                runeliteCustomJarSelect.disabled = false;
                runeliteUseCustomJar.disabled = false;
            }
        };
        xml.open('GET', "/jar-file-picker", true);
        xml.send();
    };
    runeliteUseCustomJar.onchange = () => {
        if (runeliteUseCustomJar.checked) {
            runeliteCustomJarDiv.classList.remove("opacity-25");
            runeliteCustomJar.disabled = false;
            runeliteCustomJarSelect.disabled = false;
        } else {
            runeliteCustomJarDiv.classList.add("opacity-25");
            runeliteCustomJar.disabled = true;
            runeliteCustomJarSelect.disabled = true;
        }

        config.runelite_use_custom_jar = runeliteUseCustomJar.checked;
        if (config.runelite_use_custom_jar) {
            if (runeliteUseCustomJar.persisted_path) config.runelite_custom_jar = runeliteUseCustomJar.persisted_path;
        } else {
            runeliteUseCustomJar.persisted_path = config.runelite_custom_jar;
            delete config.runelite_custom_jar;
        }
        configIsDirty = true;
    };

    runeliteUseCustomJar.checked = config.runelite_use_custom_jar || false;
    if (runeliteUseCustomJar.checked) {
        runeliteCustomJarDiv.classList.remove("opacity-25")
        runeliteCustomJar.disabled = false;
        runeliteCustomJarSelect.disabled = false;
    }
    if (config.runelite_custom_jar) runeliteCustomJar.value = config.runelite_custom_jar;

    if (platform !== "linux") {
        let flatpakDiv = document.getElementById("flatpak_div")
        flatpakDiv.remove();
    }

    accountSelect.onchange = () => {
        clearElement(gameAccountSelection);
        if (accountSelect.selectedIndex >= 0) {
            const opt = accountSelect.options[accountSelect.selectedIndex];
            generateAccountSelection(opt.genLoginVars, opt.gameAccountSelect);
            if (!isLoading) {
                config.selected_account = opt.creds.sub;
                configIsDirty = true;
            }
        }
    };

    loginButton.onclick = () => {
        if (pendingOauth === null || pendingOauth.win.closed) {
            var state = makeRandomState();
            const verifier = makeRandomVerifier();
            makeLoginUrl({
                origin: atob(s.origin),
                redirect: atob(s.redirect),
                authMethod: "",
                loginType: "",
                clientid: client_id,
                flow: "launcher",
                pkceState: state,
                pkceCodeVerifier: verifier,
            }).then((e) => {
                var win = window.open(e, "", "width=480,height=720");
                pendingOauth = { state: state, verifier: verifier, win: win };
            });
        } else {
            pendingOauth.win.focus();
        }
    };

    logoutButton.removeLogin = (opt, creds) => {
        accountSelect.removeChild(opt);
        accountSelect.selectedIndex = -1;
        clearElement(gameAccountSelection);
        credentials.splice(creds, 1);
        credentialsAreDirty = true;
        saveAllCreds();
    };
    logoutButton.onclick = () => {
        if (accountSelect.selectedIndex < 0) {
            msg("Logout unsuccessful: no account selected");
            return;
        }
        const opt = accountSelect.options[accountSelect.selectedIndex];
        const creds = opt.creds;
        checkRenewCreds(creds, exchange_url, client_id).then((x) => {
            if (x === null) {
                revokeOauthCreds(creds.access_token, revoke_url, client_id).then((res) => {
                    if (res === 200) {
                        msg("Successful logout");
                        logoutButton.removeLogin(opt, creds);
                    } else {
                        err(`Logout unsuccessful: status ${res}`);
                    }
                });
            } else if (x === 400 || x === 401) {
                msg("Logout unsuccessful: credentials are invalid, so discarding them anyway");
                logoutButton.removeLogin(opt, creds);
            } else {
                err("Logout unsuccessful: unable to verify credentials due to a network error");
            }
        });
    };

    dataDirButton.onclick = () => {
        var xml = new XMLHttpRequest();
        xml.open('GET', "/browse-data");
        xml.onreadystatechange = () => {
            if (xml.readyState == 4) {
                msg(`Browse status: '${xml.responseText.trim()}'`);
            }
        };
        xml.send();
    };

    if (creds) {
        toggle_disclaimer();
    }

    (async () => {
        if (credentials.length > 0) {
            const old_credentials_size = credentials.length;
            const promises = credentials.map(async (x) => {
                const result = await checkRenewCreds(x, exchange_url, client_id);
                if (result !== null && result !== 0) {
                    err(`Discarding expired login for #${x.sub}`, false);
                }
                if (result === null && await handleLogin(s, null, x, exchange_url, client_id)) {
                    return { creds: x, valid: true };
                } else {
                    return { creds: x, valid: result === 0 };
                }
            });
            const responses = await Promise.all(promises);
            credentials = responses.filter((x) => x.valid).map((x) => x.creds);
            credentialsAreDirty |= credentials.length != old_credentials_size;
            saveAllCreds();

            for (var i = 0; i < accountSelect.childElementCount; i += 1) {
                if (accountSelect.options[i].creds.sub === config.selected_account && accountSelect.selectedIndex != i) {
                    accountSelect.selectedIndex = i;
                    accountSelect.onchange();
                    break;
                }
            }
        }
        isLoading = false;
        configIsDirty = false; // overrides all cases where this gets set to "true" due to loading existing config values
    })();
}

// Handles a new session id as part of the login flow. Can also be called on startup with a
// persisted session id. Adds page elements and returns true on success, or returns false
// on failure.
function handleNewSessionId(s, creds, accounts_url, account_info_promise) {
    return new Promise((resolve, reject) => {
        var xml = new XMLHttpRequest();
        xml.onreadystatechange = () => {
            if (xml.readyState == 4) {
                if (xml.status == 200) {
                    account_info_promise.then((account_info) => {
                        if (typeof account_info !== "number") {
                            const name_text = `${account_info.displayName}#${account_info.suffix}`;
                            msg(`Successfully added login for ${name_text}`);
                            var select = document.createElement("select");
                            var select_index = null;
                            JSON.parse(xml.response).forEach((acc, i) => {
                                var opt = document.createElement("option");
                                opt.value = acc.accountId;
                                opt.seq = acc.accountId;
                                if (acc.displayName) {
                                    opt.name = acc.displayName;
                                    opt.innerText = acc.displayName;
                                } else {
                                    opt.innerText = `#${acc.accountId}`;
                                }
                                select.appendChild(opt);
                                if (config.selected_game_accounts && config.selected_game_accounts[creds.sub] === opt.seq) {
                                    select_index = i;
                                }
                            });
                            if (select_index) {
                                select.selectedIndex = select_index;
                            }
                            const gen_login_vars = (f, element, opt, basic_auth_header) => {
                                const acc_id = opt.value;
                                const acc_name = opt.name;
                                f(s, element, null, null, creds.session_id, acc_id, acc_name);
                            };
                            addNewAccount(name_text, creds, gen_login_vars, select);
                            resolve(true);
                        } else {
                            err(`Error getting account info: ${account_info}`, false);
                            resolve(false);
                        }
                    });
                } else {
                    err(`Error: from ${accounts_url}: ${xml.status}: ${xml.response}`, false);
                    resolve(false);
                }
            }
        };
        xml.open('GET', accounts_url, true);
        xml.setRequestHeader("Accept", "application/json");
        xml.setRequestHeader("Authorization", "Bearer ".concat(creds.session_id));
        xml.send();
    });
}

// Called on new successful login with credentials. Delegates to a specific handler based on login_provider value.
// This function is responsible for adding elements to the page (either directly or indirectly via a GameAuth message),
// however it does not save credentials. You should call saveAllCreds after calling this function any number of times.
// Returns true if the credentials should be treated as valid by the caller immediately after return, or false if not.
async function handleLogin(s, win, creds, exchange_url, client_id) {
    switch (creds.login_provider) {
        case atob(s.provider):
            if (win) {
                win.close();
            }
            return await handleGameLogin(s, creds, exchange_url, client_id);
        default:
            return await handleStandardLogin(s, win, creds, exchange_url, client_id);
    }
}

// called when login was successful, but with absent or unrecognised login_provider
async function handleStandardLogin(s, w, creds, refresh_url, client_id) {
    const state = makeRandomState();
    const nonce = crypto.randomUUID();
    const location = atob(s.origin).concat("/oauth2/auth?").concat(new URLSearchParams({
        id_token_hint: creds.id_token,
        nonce: btoa(nonce),
        prompt: "consent",
        redirect_uri: "http://localhost",
        response_type: "id_token code",
        state: state,
        client_id: production_client_id,
        scope: "openid offline"
    }));
    var account_info_promise = getStandardAccountInfo(s, creds);

    var win = w;
    if (win) {
        win.location.href = location;
        pendingGameAuth.push({
            state: state,
            nonce: nonce,
            creds: creds,
            win: win,
            account_info_promise: account_info_promise
        });
        return false;
    } else {
        if (!creds.session_id) {
            err("Rejecting stored credentials with missing session_id", false);
            return false;
        }

        return await handleNewSessionId(s, creds, atob(s.auth_api).concat("/accounts"), account_info_promise);
    }
}

// called after successful login using a game account as the login_provider
async function handleGameLogin(s, creds, refresh_url, client_id) {
    const auth_url = atob(s.profile_api).concat("/profile");
    var xml = new XMLHttpRequest();
    xml.onreadystatechange = () => {
        if (xml.readyState == 4) {
            if (xml.status == 200) {
                // note: select element is just for show here and unused in the callback,
                // since there can only be one game account in this situation
                const name_info = JSON.parse(xml.response);
                const name_text = name_info.displayNameSet ? name_info.displayName : `#${creds.sub}`;
                var select = document.createElement("select");
                var opt = document.createElement("option");
                opt.innerText = name_text;
                opt.seq = creds.sub;
                select.add(opt);
                select.selectedIndex = 0;
                msg(`Successfully added login for ${name_text}`);
                addNewAccount(name_text, creds, (f, element, opt, basic_auth_header) => {
                    getShieldTokens(creds, atob(s.shield_url), refresh_url, client_id, basic_auth_header).then((e) => {
                        if (typeof e !== "number") {
                            f(s, element, e.access_token, e.refresh_token, null, null, name_info.displayNameSet ? name_info.displayName : null);
                        } else {
                            err(`Error getting shield tokens: ${atob(s.shield_url)}: ${e}`, false);
                            element.disabled = false;
                        }
                    });
                }, select);
            } else {
                err(`Error: from ${auth_url}: ${xml.status}: ${xml.response}`, false);
            }
        }
    };
    xml.onerror = () => {
        err(`Error: ${auth_url}: non-http error`, false);
    };
    xml.open('GET', auth_url, true);
    xml.setRequestHeader("Authorization", "Bearer ".concat(creds.id_token));
    xml.send();
    return true;
}

// makes a request to the account_info endpoint and returns the promise
// the promise will return either a JSON object on success or a status code on failure
function getStandardAccountInfo(s, creds) {
    return new Promise((resolve, reject) => {
        const url = `${atob(s.api)}/users/${creds.sub}/displayName`;
        var xml = new XMLHttpRequest();
        xml.onreadystatechange = () => {
            if (xml.readyState == 4) {
                if (xml.status == 200) {
                    resolve(JSON.parse(xml.response));
                } else {
                    resolve(xml.status);
                }
            }
        }
        xml.open('GET', url, true);
        xml.setRequestHeader("Authorization", "Bearer ".concat(creds.access_token));
        xml.send();
    });
}

// use oauth creds to get a response from the "shield" endpoint
// returns a JSON object on success or a HTTP status code on failure
function getShieldTokens(creds, url, refresh_url, client_id, basic_auth_header) {
    return new Promise((resolve, reject) => {
        checkRenewCreds(creds, refresh_url, client_id).then((status) => {
            saveAllCreds();
            if (status === null) {

                var xml = new XMLHttpRequest();
                xml.open('POST', url, true);
                xml.setRequestHeader("Content-Type", "application/x-www-form-urlencoded");
                xml.setRequestHeader("Authorization", basic_auth_header);
                xml.onreadystatechange = () => {
                    if (xml.readyState == 4) {
                        if (xml.status == 200) {
                            resolve(JSON.parse(xml.response));
                        } else {
                            resolve(xml.status);
                        }
                    }
                };
                xml.send(new URLSearchParams({
                    token: creds.access_token,
                    grant_type: "token_exchange",
                    scope: "gamesso.token.create"
                }));
            } else {
                err(`Error renewing credentials: ${refresh_url}: ${status}`, false);
                resolve(status);
            }
        });
    });
}

// sends a request to save all credentials to their config file,
// overwriting the previous file, if any
async function saveAllCreds() {
    if (credentialsAreDirty) {
        var xml = new XMLHttpRequest();
        xml.open('POST', "/save-credentials", true);
        xml.setRequestHeader("Content-Type", "application/json");
        xml.onreadystatechange = () => {
            if (xml.readyState == 4) {
                msg(`Save-credentials status: ${xml.responseText.trim()}`);
            }
        };
        xml.send(JSON.stringify(credentials));
        credentialsAreDirty = false;
    }
}

// adds an option to the accounts drop-down menu at the top of the page
// genLoginVars is the function that will be passed to generateLaunchButtons
function addNewAccount(name, creds, genLoginVars, select) {
    const index = accountSelect.childElementCount;
    var opt = document.createElement("option");
    opt.genLoginVars = genLoginVars;
    opt.name = name;
    opt.innerText = name;
    opt.gameAccountSelect = select;
    opt.creds = creds;
    opt.classList = "bg-slate-100 text-slate-950 dark:bg-slate-900 dark:text-slate-50"
    accountSelect.add(opt);
    accountSelect.onchange();
}

// populates the gameAccountSelection element
// "f" is the function passed to generateLaunchButtons, "game_account_select" is an HTML select element
function generateAccountSelection(f, game_account_select) {
    clearElement(gameAccountSelection);
    for (let option of game_account_select.options) {
        option.classList = "bg-slate-100 text-slate-950 dark:bg-slate-900 dark:text-slate-50"
        gameAccountSelection.appendChild(option);
    }

    gameAccountSelection.onchange = () => {
        if (gameAccountSelection.selectedIndex >= 0) {
            generateLaunchButtonsOsrs(f, gameAccountSelection.options[gameAccountSelection.selectedIndex]);
            generateLaunchButtonsRs3(f, gameAccountSelection.options[gameAccountSelection.selectedIndex]);
            if (accountSelect.selectedIndex != -1) {
                if (!config.selected_game_accounts) config.selected_game_accounts = {};
                const key = accountSelect.options[accountSelect.selectedIndex].creds.sub;
                const val = gameAccountSelection.options[gameAccountSelection.selectedIndex].seq;
                config.selected_game_accounts[key] = val;
            }
        }
        configIsDirty = true;
    };
    gameAccountSelection.onchange();
}

// provides onclick methods to rs3 buttons
// the parameter is a function callback, which must take exactly two arguments:
// 1. a function which your function should invoke with JX env variables, e.g. launchRS3Linux
// 2. a HTML Button element, which should be passed to parameter 1 when invoking it, or, if not
//    invoking the callback for some reason, then set disabled=false on parameter 2 before returning
// 3. a HTML Option element, representing the currently selected game account
// 4. the "Basic" auth header used for legacy-style logins to this game
function generateLaunchButtonsRs3(f, opt) {
    const basic_auth = "Basic Y29tX2phZ2V4X2F1dGhfZGVza3RvcF9yczpwdWJsaWM=";

    if (platform === "linux") {
        var rs3_linux = document.getElementById("rs3_launch");
        rs3_linux.onclick = () => { rs3_linux.disabled = true; f(launchRS3Linux, rs3_linux, opt, basic_auth); };
    }
}

// provides onclick methods to osrs buttons
// the parameter is a function callback, which must take exactly two arguments:
// 1. a function which your function should invoke with JX env variables, e.g. launchRS3Linux
// 2. a HTML Button element, which should be passed to parameter 1 when invoking it, or, if not
//    invoking the callback for some reason, then set disabled=false on parameter 2 before returning
// 3. a HTML Option element, representing the currently selected game account
// 4. the "Basic" auth header used for legacy-style logins to this game
function generateLaunchButtonsOsrs(f, opt) {
    const basic_auth = "Basic Y29tX2phZ2V4X2F1dGhfZGVza3RvcF9vc3JzOnB1YmxpYw==";

    var rl = document.getElementById("rl_launch");
    rl.onclick = () => { rl.disabled = true; f(launchRuneLite, rl, opt, basic_auth); };

    var rl_config = document.getElementById("rl_configure");
    rl_config.onclick = () => { rl_config.disabled = true; f(launchRuneLiteConfigure, rl_config, opt, basic_auth); };

    var hdos = document.getElementById("hdos_launch");
    hdos.onclick = () => { hdos.disabled = true; f(launchHdos, hdos, opt, basic_auth); };
}

// asynchronously download and launch RS3's official .deb client using the given env variables
function launchRS3Linux(s, element, jx_access_token, jx_refresh_token, jx_session_id, jx_character_id, jx_display_name) {
    saveConfig();

    const launch = (hash, deb) => {
        var xml = new XMLHttpRequest();
        var params = {};
        if (hash) params.hash = hash;
        if (jx_access_token) params.jx_access_token = jx_access_token;
        if (jx_refresh_token) params.jx_refresh_token = jx_refresh_token;
        if (jx_session_id) params.jx_session_id = jx_session_id;
        if (jx_character_id) params.jx_character_id = jx_character_id;
        if (jx_display_name) params.jx_display_name = jx_display_name;
        if (config.rs_config_uri) {
            params.config_uri = config.rs_config_uri;
        } else {
            params.config_uri = atob(s.default_config_uri);
        }
        xml.open('POST', "/launch-rs3-deb?".concat(new URLSearchParams(params)), true);
        xml.onreadystatechange = () => {
            if (xml.readyState == 4) {
                msg(`Game launch status: '${xml.responseText.trim()}'`);
                if (xml.status == 200 && hash) {
                    rs3LinuxInstalledHash = hash;
                }
                element.disabled = false;
            }
        };
        xml.send(deb);
    };

    var xml = new XMLHttpRequest();
    const content_url = atob(s.content_url);
    const url = content_url.concat("dists/trusty/non-free/binary-amd64/Packages");
    xml.open('GET', url, true);
    xml.onreadystatechange = () => {
        if (xml.readyState == 4 && xml.status == 200) {
            const lines = Object.fromEntries(xml.response.split('\n').map((x) => { return x.split(": "); }));
            if (!lines.Filename || !lines.Size) {
                err(`Could not parse package data from URL: ${url}`, false);
                launch();
                return;
            }
            if (lines.SHA256 !== rs3LinuxInstalledHash) {
                var m = msg("Downloading game client... 0%");
                var exe_xml = new XMLHttpRequest();
                exe_xml.open('GET', content_url.concat(lines.Filename), true);
                exe_xml.responseType = "arraybuffer";
                exe_xml.onprogress = (e) => {
                    if (e.loaded) {
                        m.innerText = `Downloading game client... ${(Math.round(1000.0 * e.loaded / lines.Size) / 10.0).toFixed(1)}%`;
                    }
                };
                exe_xml.onreadystatechange = () => {
                    if (exe_xml.readyState == 4 && exe_xml.status == 200) {
                        launch(lines.SHA256, exe_xml.response);
                    }
                };
                exe_xml.onerror = () => {
                    err(`Error downloading game client: from ${url}: non-http error`, false);
                    launch();
                };
                exe_xml.send();
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

// locate runelite's .jar either from the user's config or by parsing github releases,
// then attempt to launch it with the given env variables
// last param indices whether --configure will be passed or not
function launchRuneLiteInner(s, element, jx_access_token, jx_refresh_token, jx_session_id, jx_character_id, jx_display_name, configure) {
    saveConfig();
    const launch_path = configure ? "/launch-runelite-jar-configure?" : "/launch-runelite-jar?";

    const launch = (id, jar, jar_path) => {
        var xml = new XMLHttpRequest();
        var params = {};
        if (id) params.id = id;
        if (jar_path) params.jar_path = jar_path;
        if (jx_access_token) params.jx_access_token = jx_access_token;
        if (jx_refresh_token) params.jx_refresh_token = jx_refresh_token;
        if (jx_session_id) params.jx_session_id = jx_session_id;
        if (jx_character_id) params.jx_character_id = jx_character_id;
        if (jx_display_name) params.jx_display_name = jx_display_name;
        if (runeliteFlatpakRichPresence.checked) params.flatpak_rich_presence = "";
        xml.open(jar ? 'POST' : 'GET', launch_path.concat(new URLSearchParams(params)), true);
        xml.onreadystatechange = () => {
            if (xml.readyState == 4) {
                msg(`Game launch status: '${xml.responseText.trim()}'`);
                if (xml.status == 200 && id) {
                    runeliteInstalledID = id;
                }
                element.disabled = false;
            }
        };
        xml.send(jar);
    };

    if (runeliteUseCustomJar.checked) {
        launch(null, null, runeliteCustomJar.value);
        return;
    }

    var xml = new XMLHttpRequest();
    const url = "https://api.github.com/repos/runelite/launcher/releases";
    xml.open('GET', url, true);
    xml.onreadystatechange = () => {
        if (xml.readyState == 4) {
            if (xml.status == 200) {
                const runelite = JSON.parse(xml.responseText).map((x) => x.assets).flat().find((x) => x.name.toLowerCase() == "runelite.jar");
                if (runelite.id != runeliteInstalledID) {
                    var m = msg("Downloading RuneLite...");
                    var xml_rl = new XMLHttpRequest();
                    xml_rl.open('GET', runelite.browser_download_url, true);
                    xml_rl.responseType = "arraybuffer";
                    xml_rl.onreadystatechange = () => {
                        if (xml_rl.readyState == 4) {
                            if (xml_rl.status == 200) {
                                launch(runelite.id, xml_rl.response);
                            } else {
                                err(`Error downloading from ${runelite.url}: ${xml_rl.status}: ${xml_rl.responseText}`);
                                element.disabled = false;
                            }
                        }
                    };
                    xml_rl.onprogress = (e) => {
                        if (e.loaded && e.lengthComputable) {
                            m.innerText = `Downloading RuneLite... ${(Math.round(1000.0 * e.loaded / e.total) / 10.0).toFixed(1)}%`;
                        }
                    };
                    xml_rl.send();
                } else {
                    msg("Latest JAR is already installed");
                    launch();
                }
            } else {
                err(`Error from ${url}: ${xml.status}: ${xml.responseText}`, false);
                element.disabled = false;
            }
        }
    }
    xml.send();
}

function launchRuneLite(s, element, jx_access_token, jx_refresh_token, jx_session_id, jx_character_id, jx_display_name) {
    return launchRuneLiteInner(s, element, jx_access_token, jx_refresh_token, jx_session_id, jx_character_id, jx_display_name, false);
}

function launchRuneLiteConfigure(s, element, jx_access_token, jx_refresh_token, jx_session_id, jx_character_id, jx_display_name) {
    return launchRuneLiteInner(s, element, jx_access_token, jx_refresh_token, jx_session_id, jx_character_id, jx_display_name, true);
}

// locate hdos's .jar from their CDN, then attempt to launch it with the given env variables
function launchHdos(s, element, jx_access_token, jx_refresh_token, jx_session_id, jx_character_id, jx_display_name) {
    saveConfig();

    const launch = (version, jar) => {
        var xml = new XMLHttpRequest();
        var params = {};
        if (version) params.version = version;
        if (jx_access_token) params.jx_access_token = jx_access_token;
        if (jx_refresh_token) params.jx_refresh_token = jx_refresh_token;
        if (jx_session_id) params.jx_session_id = jx_session_id;
        if (jx_character_id) params.jx_character_id = jx_character_id;
        if (jx_display_name) params.jx_display_name = jx_display_name;
        xml.open('POST', "/launch-hdos-jar?".concat(new URLSearchParams(params)), true);
        xml.onreadystatechange = () => {
            if (xml.readyState == 4) {
                msg(`Game launch status: '${xml.responseText.trim()}'`);
                if (xml.status == 200 && version) {
                    hdosInstalledVersion = version;
                }
                element.disabled = false;
            }
        };
        xml.send(jar);
    };

    var xml = new XMLHttpRequest();
    const url = "https://cdn.hdos.dev/client/getdown.txt";
    xml.open('GET', url, true);
    xml.onreadystatechange = () => {
        if (xml.readyState == 4) {
            if (xml.status == 200) {
                const version_regex = xml.responseText.match(/^launcher\.version *= *(.*?)$/m);
                if (version_regex || version_regex.length == 2) {
                    const latest_version = version_regex[1];
                    if (latest_version !== hdosInstalledVersion) {
                        const jar_url = `https://cdn.hdos.dev/launcher/v${latest_version}/hdos-launcher.jar`;
                        var m = msg("Downloading HDOS...");
                        var xml_hdos = new XMLHttpRequest();
                        xml_hdos.open('GET', jar_url, true);
                        xml_hdos.responseType = "arraybuffer";
                        xml_hdos.onreadystatechange = () => {
                            if (xml_hdos.readyState == 4) {
                                if (xml_hdos.status == 200) {
                                    launch(latest_version, xml_hdos.response);
                                } else {
                                    err(`Error downloading from ${runelite.url}: ${xml_hdos.status}: ${xml_hdos.responseText}`);
                                    element.disabled = false;
                                }
                            }
                        };
                        xml_hdos.onprogress = (e) => {
                            if (e.loaded && e.lengthComputable) {
                                m.innerText = `Downloading HDOS... ${(Math.round(1000.0 * e.loaded / e.total) / 10.0).toFixed(1)}%`;
                            }
                        };
                        xml_hdos.send();
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
                element.disabled = false;
            }
        }
    }
    xml.send();
}

// revokes the given oauth tokens, returning an http status code.
// tokens were revoked only if response is 200
function revokeOauthCreds(access_token, revoke_url, client_id) {
    return new Promise((resolve, reject) => {
        var xml = new XMLHttpRequest();
        xml.open('POST', revoke_url, true);
        xml.onreadystatechange = () => {
            if (xml.readyState == 4) {
                resolve(xml.status);
            }
        };
        xml.setRequestHeader("Content-Type", "application/x-www-form-urlencoded");
        xml.send(new URLSearchParams({ token: access_token, client_id: client_id }));
    });
}

// sends an asynchronous request to save the current user config to disk, if it has changed
var saveConfigRunning = false;
function saveConfig() {
    if (configIsDirty && !saveConfigRunning) {
        saveConfigRunning = true;
        var xml = new XMLHttpRequest();
        xml.open('POST', "/save-config", true);
        xml.onreadystatechange = () => {
            if (xml.readyState == 4) {
                msg(`Save config status: '${xml.responseText.trim()}'`);
                if (xml.status == 200) {
                    configIsDirty = false;
                }
                saveConfigRunning = false;
            }
        };
        xml.setRequestHeader("Content-Type", "application/json");
        xml.send(JSON.stringify(config, null, 4));
    }
}

// clears all child content from an element
function clearElement(e) {
    while (e.lastElementChild) {
        e.removeChild(e.lastElementChild);
    }
}

// adds a message to the message list, returning the inner <li> element
function msg(str) {
    console.log(str);
    return insertMessage(str);
}

// adds an error message to the message list
// if do_throw is true, throws the error message, otherwise returns the new <li> element
function err(str, do_throw) {
    if (!do_throw) {
        console.error(str);
    }
    var li = insertMessage(str);
    li.setAttribute("class", "text-rose-500");
    if (do_throw) {
        throw new Error(str);
    } else {
        return li;
    }
}

// inserts a message into the message list and returns the new <li> element
// don't call this directly, call msg or err instead
function insertMessage(str) {
    var li = document.createElement("li");
    li.innerHTML = str
    messages.insertBefore(li, messages.firstChild);
    while (messages.childElementCount > 20) {
        messages.removeChild(messages.lastChild);
    }
    return li;
}

onload = () => start(s());
onunload = saveConfig;
