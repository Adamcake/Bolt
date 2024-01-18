# Launcher HTML
The launcher UI is a webpage served from the files in this directory, which are installed alongside Bolt binaries. The "entry point" is `launcher.html`. This readme documents everything that's provided by the application for the purposes of UI creation.

## Query Params
The initial request to `launcher.html` may have any of the following query params:
- `platform`: the platform this was built for. Currently, may only be "windows", "mac" or "linux". This param will always be present.
- `credentials`: a JSON-encoded string containing an array of objects, one for each account the user is signed in on. Contains access_token, id_token, refresh_token, expiry, login_provider, and sub (unique account ID). If this param is absent, the user has no credentials file.
- `config`: a JSON-encoded string containing the user config. Anything at all may be stored in the config object; when the object is sent in a `/save-config` request (described later in this readme), the same object will be present in this param next time Bolt opens. If this param is absent, the user has no config file.
- `flathub`: a boolean indicating whether this is a flathub build, useful for making error messages more helpful. Assume false if not present.
- `rs3_linux_installed_hash`: if RS3 is installed, this param will be present indicating the hash of the .deb from which it was installed. Used for update-checking by comparing the hash against the one found in the metafile of the official download repo.
- `runelite_installed_id` - if RuneLite is installed, this param will be present indicating the unique ID of the [Github asset](https://api.github.com/repos/runelite/launcher/releases) from which the JAR was downloaded. Used for update-checking.
- `hdos_installed_version` - if HDOS is installed, this param will be present indicating the value `launcher.version` from the [getdown config](https://cdn.hdos.dev/client/getdown.txt) at the time when it was installed. Used for update-checking.

## Request API
The following tasks are achieved by making web requests to `https://bolt-internal/`. All queries will respond with 200 if the request is successful, or otherwise 4xx or 5xx with a plain text error message. The requests are as follows:
- `/close`: [deprecated] causes the process to exit.
- `/save-credentials`: saves user credentials to a file, so they may be restored and passed via the `credentials` param next time the program starts. This request must be a POST. Any POST data will be saved to the file as-is, overwriting the existing file if any. This request should be made immediately after the credentials object changes, since almost any change completely invalidates the old file contents.
- `/save-config`: saves user configuration to a file, so it may be restored and passed via the `config` param next time the program starts. This request must be a POST. Any POST data will be saved to the file as-is, overwriting the existing file if any. Unlike credentials, config changes are a fairly low priority and can be left until some time later, such as the `onunload` event.
- `/open-external-url`: attempts to open a URL in the user's browser. This request must be a POST, with the POST data containing the URL to open. The URL must start with `http://` or `https://`.
- `/jar-file-picker`: shows the user a file-picker for .jar files. The response will contain an absolute file path in plain text, unless the user closed the dialog without choosing a file, in which case the response code will 204 and the response body should be ignored.
  - Note: this request can take a very long time to complete since it waits indefinitely for user input, so do not run it synchronously and do not specify a timeout.
- `/launch-rs3-deb`: launches RS3 from the linux .deb file. Should only be used on platforms which support x86_64 ELF binaries (i.e. linux). May have the following query params:
  - `jx_...`: see "JX Variables" section
  - `hash`: a hash of a newer version of the game client to install. If set, there must also be POST data containing the downloaded contents of the .deb file. The .deb will be extracted, saved and launched. If all of that is successful, `rs3_linux_installed_hash` will be updated with the new hash.
  - `config_uri`: a string to pass as the `--configURI` command-line argument. If absent, none will be passed.
- `/launch-runelite-jar`: launches RuneLite from a JAR file. May have the following query params:
  - `jx_...`: see "JX Variables" section
  - `id`: an ID of a newer game version of the JAR to install (see `runelite_installed_id` above for where this ID is obtained.) If set, there must also be POST data containing the downloaded contents of the JAR file. The JAR will be saved and launched. If successful, `runelite_installed_id` will be updated with the new ID.
  - `jar_path`: an absolute path to a JAR file, which should be obtained from `/jar-file-picker`. If this is set, the given JAR file will be launched, bypassing any installed version. Should not be passed at the same time as `id`.
  - `scale`: value to pass to the `--scale` command-line argument. If absent, none will be passed.
  - `flatpak_rich_presence`: boolean indicating whether to expose the game's rich presence to Flatpak Discord by symlinking discord-ipc-0. Will assume false if not present. Doesn't do anything on platforms other than linux.
- `/launch-hdos-jar`: launches HDOS from a JAR file. May have the following query params:
  - `jx_...`: see "JX Variables" section
  - `version`: a version of a newer launcher version to install (see `hdos_installed_version` above for where this number is obtained.) If set, there must also be POST data containing the downloaded contents of the JAR file. The JAR will be saved and launched. If successful, `hdos_installed_version` will be updated with the new version.

## JX Variables
The following variables are used to pass authentication info to a game when launching it. They're the same across all games and clients.
- `jx_access_token`: access token for legacy-type accounts
- `jx_refresh_token`: login refresh token for legacy-type accounts
- `jx_session_id`: Session ID obtained from OAuth token exchange; stored as `session_id` in credentials object
- `jx_character_id`: The ID of the unique game character the user wants to log into - not used for legacy accounts, since they contain only a single game account
- `jx_display_name`: the display name of the account being logged into - optional, should be left unset if the account has no display name according to the login server
