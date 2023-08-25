const internal_url = "https://bolt-internal";
var platform = null;
var credentials = [];
var credentialsAreDirty = false;
var pendingOauth = null;
var pendingGameAuth = [];
var rs3LinuxInstalledHash = null;
var runeliteInstalledHash = null;
var config = {};
var configIsDirty = false;
var isLoading = true;

// globally-available root elements
var messages = document.createElement("ul");
var accountSelect = document.createElement("select");
var loginButton = document.createElement("button");
var logoutButton = document.createElement("button");
var loggedInInfo = document.createElement("div");
var loginButtons = document.createElement("div");
var gameAccountSelection = document.createElement("div");
var footer = document.createElement("div");

// config setting elements
var runeliteUseCustomJar = document.createElement("input");
var runeliteCustomJar = document.createElement("textarea");

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
    return Array.from(n, function(e) {
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

// body's onload function
function start(s) {
    const s_origin = atob(s.origin);
    const client_id = atob(s.clientid);
    const exchange_url = s_origin.concat("/oauth2/token");
    const revoke_url = s_origin.concat("/oauth2/revoke");

    const query = new URLSearchParams(window.location.search);
    platform = query.get("platform");
    rs3LinuxInstalledHash = query.get("rs3_linux_installed_hash");
    runeliteInstalledHash = query.get("runelite_installed_hash");
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

    window.addEventListener("message", (event) => {
        if (event.origin != internal_url && btoa(event.origin).replaceAll('\x3d', '') != s.origin) {
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
                msg(`message: external url: ${event.data.url}`);
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

    var launchGameButtons = document.createElement("div");
    var settingsOsrs = document.createElement("div");
    var settingsRs3 = document.createElement("div");

    var runeliteCustomJarLabel = document.createElement("label");
    runeliteCustomJarLabel.innerText = "Use custom RuneLite JAR: ";
    runeliteCustomJarLabel.for = runeliteUseCustomJar;

    runeliteUseCustomJar.type = "checkbox";
    runeliteUseCustomJar.checked = config.runelite_use_custom_jar || false;

    runeliteCustomJar.disabled = true;
    runeliteCustomJar.setAttribute("rows", 1);
    if (config.runelite_custom_jar) runeliteCustomJar.value = config.runelite_custom_jar;

    var runeliteCustomJarSelect = document.createElement("button");
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
                }
                runeliteCustomJarSelect.disabled = false;
                runeliteUseCustomJar.disabled = false;
            }
        };
        xml.open('GET', "/jar-file-picker", true);
        xml.send();
    };
    runeliteCustomJarSelect.innerText = "Select File";
    runeliteUseCustomJar.onchange = () => {
        runeliteCustomJarSelect.disabled = !runeliteUseCustomJar.checked;
        config.runelite_use_custom_jar = runeliteUseCustomJar.checked;
        if (config.runelite_use_custom_jar) {
            if (runeliteUseCustomJar.persisted_path) config.runelite_custom_jar = runeliteUseCustomJar.persisted_path;
        } else {
            runeliteUseCustomJar.persisted_path = config.runelite_custom_jar;
            delete config.runelite_custom_jar;
        }
        configIsDirty = true;
    };
    runeliteCustomJarSelect.disabled = !runeliteUseCustomJar.checked;

    settingsOsrs.appendChild(runeliteCustomJarLabel);
    settingsOsrs.appendChild(runeliteUseCustomJar);
    settingsOsrs.appendChild(runeliteCustomJar);
    settingsOsrs.appendChild(runeliteCustomJarSelect);

    var accounts_label = document.createElement("label");
    accounts_label.innerText = "Logged in as:";
    accounts_label.for = accountSelect;
    loggedInInfo.appendChild(accounts_label);
    loggedInInfo.appendChild(accountSelect);

    var game_select = document.createElement("select");
    s.games.forEach((x, i) => {
        var opt = document.createElement("option");
        opt.name = atob(x);
        opt.innerText = opt.name;
        opt.value = i;
        switch (i) {
            case 0:
                opt.genLaunchButtons = generateLaunchButtonsRs3;
                opt.settingsElement = settingsRs3;
                break;
            case 1:
                opt.genLaunchButtons = generateLaunchButtonsOsrs;
                opt.settingsElement = settingsOsrs;
                break;
        }
        game_select.appendChild(opt);
    });
    if (config.selected_game_index) {
        game_select.selectedIndex = config.selected_game_index;
    }

    accountSelect.onchange = () => {
        clearElement(launchGameButtons);
        clearElement(gameAccountSelection);
        if (accountSelect.selectedIndex >= 0) {
            const opt = accountSelect.options[accountSelect.selectedIndex];
            generateAccountSelection(opt.genLoginVars, opt.gameAccountSelect, game_select, launchGameButtons);
        }
    };

    loginButton.innerText = "Log in";
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

    logoutButton.innerText = "Log out";
    logoutButton.setAttribute("class", "button-red");
    logoutButton.removeLogin = (opt, creds) => {
        accountSelect.removeChild(opt);
        accountSelect.selectedIndex = -1;
        clearElement(gameAccountSelection);
        clearElement(launchGameButtons);
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

    var text = document.createElement("text");
    text.innerText = " | ";

    var p = document.createElement("p");
    p.innerText = "Messages:";
    loginButtons.appendChild(loginButton);
    loginButtons.appendChild(text);
    loginButtons.appendChild(logoutButton);
    document.body.appendChild(loginButtons);
    document.body.appendChild(loggedInInfo);
    document.body.appendChild(document.createElement("br"));
    document.body.appendChild(document.createElement("hr"));
    document.body.appendChild(gameAccountSelection);
    document.body.appendChild(launchGameButtons);
    document.body.appendChild(document.createElement("br"));
    document.body.appendChild(document.createElement("hr"));
    document.body.appendChild(p);
    document.body.appendChild(messages);

    var loading = document.createElement("div");
    loading.setAttribute("class", "div-bg");
    document.body.appendChild(loading);

    if (!creds) {
        var disclaimer = document.createElement("div");
        var close_disclaimer = document.createElement("button");
        disclaimer.setAttribute("class", "div-bg");
        close_disclaimer.setAttribute("class", "button-disclaimer");
        close_disclaimer.innerText = "I understand";
        close_disclaimer.onclick = () => {
            disclaimer.remove();
        };
        var p1 = document.createElement("p");
        p1.setAttribute("class", "p-disclaimer");
        p1.innerHTML = atob("Qm9sdCBpcyBhbiA8Yj51bm9mZmljaWFsIHRoaXJkLXBhcnR5IGxhdW5jaGVyPC9iPi4gSXQncyBmcmVlIGFuZCBvcGVuLXNvdXJjZSBzb2Z0d2FyZSBsaWNlbnNlZCB1bmRlciBBR1BMIDMuMC4=");
        var p2 = document.createElement("p");
        p2.setAttribute("class", "p-disclaimer");
        p2.innerHTML = atob("SmFnZXggaXMgPGI+bm90IHJlc3BvbnNpYmxlPC9iPiBmb3IgYW55IHByb2JsZW1zIG9yIGRhbWFnZSBjYXVzZWQgYnkgdXNpbmcgdGhpcyBwcm9kdWN0Lg");
        disclaimer.appendChild(document.createElement("br"));
        disclaimer.appendChild(p1);
        disclaimer.appendChild(p2);
        disclaimer.appendChild(close_disclaimer);
        document.body.appendChild(disclaimer);
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
        }
        isLoading = false;
        loading.remove();
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
        client_id: "1fddee4e-b100-4f4e-b2b0-097f9088f9d2",
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
    accountSelect.add(opt);
    accountSelect.selectedIndex = index;
    accountSelect.onchange();
}

// populates the gameAccountSelection element and by extension the launchGameButtons element
// "f" is the function passed to generateLaunchButtons, "select" and "game_select" are HTML Select elements,
// and "launchGameButtons" is a HTML Div element to be targeted by these buttons' callbacks
function generateAccountSelection(f, game_account_select, game_select, launchGameButtons) {
    clearElement(gameAccountSelection);
    var label = document.createElement("label");
    label.for = game_account_select;
    label.innerText = "Choose a game account and game:";
    game_account_select.onchange = () => {
        clearElement(launchGameButtons);
        if (game_account_select.selectedIndex >= 0 && game_select.selectedIndex >= 0) {
            const opt = game_select.options[game_select.selectedIndex];
            opt.genLaunchButtons(f, game_account_select.options[game_account_select.selectedIndex], launchGameButtons, opt.settingsElement);
            if (accountSelect.selectedIndex != -1) {
                if (!config.selected_game_accounts) config.selected_game_accounts = {};
                const key = accountSelect.options[accountSelect.selectedIndex].creds.sub;
                const val = game_account_select.options[game_account_select.selectedIndex].seq;
                config.selected_game_accounts[key] = val;
            }
        }
        config.selected_game_index = game_select.selectedIndex;        
        configIsDirty = true;
    };
    game_select.onchange = game_account_select.onchange;
    game_account_select.onchange();
    gameAccountSelection.appendChild(label);
    gameAccountSelection.appendChild(game_account_select);
    gameAccountSelection.appendChild(game_select);
}

// populates the launchGameButtons element with buttons
// the parameter is a function callback, which must take exactly two arguments:
// 1. a function which your function should invoke with JX env variables, e.g. launchRS3Linux
// 2. a HTML Button element, which should be passed to parameter 1 when invoking it, or, if not
//    invoking the callback for some reason, then set disabled=false on parameter 2 before returning
// 3. a HTML Option element, representing the currently selected game account
function generateLaunchButtonsRs3(f, opt, target, settings) {
    target.appendChild(settings);

    if (platform === "linux") {
        var rs3_linux = document.createElement("button");
        rs3_linux.onclick = () => { rs3_linux.disabled = true; f(launchRS3Linux, rs3_linux, opt, "Basic Y29tX2phZ2V4X2F1dGhfZGVza3RvcF9yczpwdWJsaWM="); };
        rs3_linux.innerText = "Launch RS3";
        target.appendChild(rs3_linux);
    }
}

// populates the launchGameButtons element with buttons
// the parameter is a function callback, which must take exactly two arguments:
// 1. a function which your function should invoke with JX env variables, e.g. launchRS3Linux
// 2. a HTML Button element, which should be passed to parameter 1 when invoking it, or, if not
//    invoking the callback for some reason, then set disabled=false on parameter 2 before returning
// 3. a HTML Option element, representing the currently selected game account
function generateLaunchButtonsOsrs(f, opt, target, settings) {
    target.appendChild(settings);

    var rl_linux = document.createElement("button");
    rl_linux.onclick = () => { rl_linux.disabled = true; f(launchRunelite, rl_linux, opt, "Basic Y29tX2phZ2V4X2F1dGhfZGVza3RvcF9vc3JzOnB1YmxpYw=="); };
    rl_linux.innerText = "Launch Runelite";
    target.appendChild(rl_linux);
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

// locate runelite's .jar either from the user's config or by downloading and caching from flatpak,
// then attempt to launch it with the given env variables
function launchRunelite(s, element, jx_access_token, jx_refresh_token, jx_session_id, jx_character_id, jx_display_name) {
    saveConfig();

    const launch = (hash, jar, jar_path) => {
        var xml = new XMLHttpRequest();
        var params = {};
        if (hash) params.hash = hash;
        if (jar_path) params.jar_path = jar_path;
        if (jx_access_token) params.jx_access_token = jx_access_token;
        if (jx_refresh_token) params.jx_refresh_token = jx_refresh_token;
        if (jx_session_id) params.jx_session_id = jx_session_id;
        if (jx_character_id) params.jx_character_id = jx_character_id;
        if (jx_display_name) params.jx_display_name = jx_display_name;
        xml.open(jar ? 'POST': 'GET', "/launch-runelite-jar?".concat(new URLSearchParams(params)), true);
        xml.onreadystatechange = () => {
            if (xml.readyState == 4) {
                msg(`Game launch status: '${xml.responseText.trim()}'`);
                if (xml.status == 200 && hash) {
                    runeliteInstalledHash = hash;
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
    const url = "https://raw.githubusercontent.com/flathub/net.runelite.RuneLite/master/net.runelite.RuneLite.json";
    xml.open('GET', url, true);
    xml.onreadystatechange = () => {
        if (xml.readyState == 4) {
            if (xml.status == 200) {
                const runelite = JSON.parse(xml.responseText).modules.find((x) => x.name == "runelite").sources.find((x) => x.type == "file" && x.sha256 && x.url && x.url.startsWith("https://github.com/runelite/launcher/"));
                if (runelite.sha256 !== runeliteInstalledHash) {
                    var m = msg("Downloading RuneLite...");
                    var xml_rl = new XMLHttpRequest();
                    xml_rl.open('GET', runelite.url, true);
                    xml_rl.responseType = "arraybuffer";
                    xml_rl.onreadystatechange = () => {
                        if (xml_rl.readyState == 4) {
                            if (xml_rl.status == 200) {
                                launch(runelite.sha256, xml_rl.response);
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

// adds a message to the message list, returning the inner <p> element
function msg(str) {
    console.log(str);
    return insertMessage(str);
}

// adds an error message to the message list
// if do_throw is true, throws the error message, otherwise returns the new <p> element
function err(str, do_throw) {
    if (!do_throw) {
        console.error(str);
    }
    var p = insertMessage(str);
    p.setAttribute("class", "p-err");
    if (do_throw) {
        throw new Error(str);
    } else {
        return p;
    }
}

// inserts a message into the message list and returns the new <p> element
// don't call this directly, call msg or err instead
function insertMessage(str) {
    var p = document.createElement("p");
    p.innerText = str;
    var li = document.createElement("li");
    li.appendChild(p);
    messages.insertBefore(li, messages.firstChild);
    while (messages.childElementCount > 20) {
        messages.removeChild(messages.lastChild);
    }
    return p;
}

onload = () => start(s());
onunload = saveConfig;
