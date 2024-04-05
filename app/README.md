# Table of Contents

- [Table of Contents](#table-of-contents)
- [Launcher](#launcher)
  - [Query Params](#query-params)
  - [Request API](#request-api)
  - [JX Variables](#jx-variables)
- [App](#app)
  - [Developing](#developing)
    - [Svelte](#svelte)
    - [tailwindcss](#tailwindcss)
    - [Linting \& Formatting](#linting--formatting)
  - [Building](#building)
    - [Minifying](#minifying)

# Launcher

The launcher UI is a webpage served from the files in the app/dist directory, which are installed alongside Bolt binaries. The "entry point" is `index.html`. This README documents everything that's provided by the application for the purposes of UI creation.

## Query Params

The initial request to `launcher.html` may have any of the following query params:

-   `platform`: the platform this was built for. Currently, may only be "windows", "mac" or "linux". This param will always be present.
-   `credentials`: a JSON-encoded string containing an array of objects, one for each account the user is signed in on. Contains access_token, id_token, refresh_token, expiry, login_provider, and sub (unique account ID). If this param is absent, the user has no credentials file.
-   `config`: a JSON-encoded string containing the user config. Anything at all may be stored in the config object; when the object is sent in a `/save-config` request (described later in this readme), the same object will be present in this param next time Bolt opens. If this param is absent, the user has no config file.
-   `flathub`: a boolean indicating whether this is a flathub build, useful for making error messages more helpful. Assume false if not present.
-   `rs3_linux_installed_hash`: if RS3 is installed, this param will be present indicating the hash of the .deb from which it was installed. Used for update-checking by comparing the hash against the one found in the metafile of the official download repo.
-   `runelite_installed_id` - if RuneLite is installed, this param will be present indicating the unique ID of the [Github asset](https://api.github.com/repos/runelite/launcher/releases) from which the JAR was downloaded. Used for update-checking.
-   `hdos_installed_version` - if HDOS is installed, this param will be present indicating the value `launcher.version` from the [getdown config](https://cdn.hdos.dev/client/getdown.txt) at the time when it was installed. Used for update-checking.

## Request API

The following tasks are achieved by making web requests to `https://bolt-internal/`. All queries will respond with 200 if the request is successful, or otherwise 4xx or 5xx with a plain text error message. The requests are as follows:

-   `/close`: [deprecated] causes the process to exit.
-   `/save-credentials`: saves user credentials to a file, so they may be restored and passed via the `credentials` param next time the program starts. This request must be a POST. Any POST data will be saved to the file as-is, overwriting the existing file if any. This request should be made immediately after the credentials object changes, since almost any change completely invalidates the old file contents.
-   `/save-config`: saves user configuration to a file, so it may be restored and passed via the `config` param next time the program starts. This request must be a POST. Any POST data will be saved to the file as-is, overwriting the existing file if any. Unlike credentials, config changes are a fairly low priority and can be left until some time later, such as the `onunload` event.
-   `/open-external-url`: attempts to open a URL in the user's browser. This request must be a POST, with the POST data containing the URL to open. The URL must start with `http://` or `https://`.
-   `/browse-data`: attempts to open Bolt's data directory in the user's file explorer.
-   `/jar-file-picker`: shows the user a file-picker for .jar files. The response will contain an absolute file path in plain text, unless the user closed the dialog without choosing a file, in which case the response code will 204 and the response body should be ignored.
    -   Note: this request can take a very long time to complete since it waits indefinitely for user input, so do not run it synchronously and do not specify a timeout.
-   `/launch-rs3-deb`: launches RS3 from the linux .deb file. Should only be used on platforms which support x86_64 ELF binaries (i.e. linux). May have the following query params:
    -   `jx_...`: see "JX Variables" section
    -   `hash`: a hash of a newer version of the game client to install. If set, there must also be POST data containing the downloaded contents of the .deb file. The .deb will be extracted, saved and launched. If all of that is successful, `rs3_linux_installed_hash` will be updated with the new hash.
    -   `config_uri`: a string to pass as the `--configURI` command-line argument. If absent, none will be passed.
-   `/launch-runelite-jar`: launches RuneLite from a JAR file. May have the following query params:
    -   `jx_...`: see "JX Variables" section
    -   `id`: an ID of a newer game version of the JAR to install (see `runelite_installed_id` above for where this ID is obtained.) If set, there must also be POST data containing the downloaded contents of the JAR file. The JAR will be saved and launched. If successful, `runelite_installed_id` will be updated with the new ID.
    -   `jar_path`: an absolute path to a JAR file, which should be obtained from `/jar-file-picker`. If this is set, the given JAR file will be launched, bypassing any installed version. Should not be passed at the same time as `id`.
    -   `flatpak_rich_presence`: boolean indicating whether to expose the game's rich presence to Flatpak Discord by symlinking discord-ipc-0. Will assume false if not present. Doesn't do anything on platforms other than linux.
-   `/launch-hdos-jar`: launches HDOS from a JAR file. May have the following query params:
    -   `jx_...`: see "JX Variables" section
    -   `version`: a version of a newer launcher version to install (see `hdos_installed_version` above for where this number is obtained.) If set, there must also be POST data containing the downloaded contents of the JAR file. The JAR will be saved and launched. If successful, `hdos_installed_version` will be updated with the new version.

## JX Variables

The following variables are used to pass authentication info to a game when launching it. They're the same across all games and clients.

-   `jx_access_token`: access token for legacy-type accounts
-   `jx_refresh_token`: login refresh token for legacy-type accounts
-   `jx_session_id`: Session ID obtained from OAuth token exchange; stored as `session_id` in credentials object
-   `jx_character_id`: The ID of the unique game character the user wants to log into - not used for legacy accounts, since they contain only a single game account
-   `jx_display_name`: the display name of the account being logged into - optional, should be left unset if the account has no display name according to the login server

# App

## Developing

### Svelte

This app is developed using [Svelte](https://svelte.dev/docs/introduction).  
They recommend using SvelteKit over base Svelte, but for this project, it made sense to keep things simple.  
Svelte uses [Vite](https://vitejs.dev/guide/why.html) under the hood, which is a fantastic build and testing tool.  
This app also uses [TypeScript](https://www.typescriptlang.org/docs/handbook/intro.html) over JavaScript. There are plenty of reasons for this, check out their site for more information!  

This was mentioned in the other README, but in case you missed it:  
Instead of `npm` and a `package-lock.json`, the frontend uses `bun` with a `bun.lockb`. Checkout [Bun](https://bun.sh/docs) to see why!  
Bun can be easily installed using npm:  
```bash
npm install -g bun
```

Begin by running `bun install`, this will install all necessary dependencies for development and release.  
Take a look inside the `package.json` file, this will show you all the different packages being used as well as the commands you can run using `bun run [COMMAND]`.

Because we are developing inside of [CEF](https://github.com/chromiumembedded/cef), here are some recommendation when doing development:

-   Use `-D BOLT_HTML_DIR=/app/dist`, `-D BOLT_DEV_SHOW_DEVTOOLS=1`, and `-D BOLT_DEV_LAUNCHER_DIRECTORY=1` when initialising cmake. This will allow us to debug and take advantage of hot reloading when we make changes in our files.
-   Use `bun run watch`. This is a wrapper for `vite build --watch`. The reason we prefer this over `bun run dev` is because CEF wants plain html, js, and css files. Perhaps there is a way to get the dev server working with CEF, but building after every change is fast enough (100ms or less).

General folder structure:

-   `app/` contains config files and the entry point, `index.html`.
    -   `app/src` has all the .svelte and .ts files that are relevant for the app.
        -   `app/src/lib` is where the components of the app live.
        -   `app/src/assets` currently only has the .css files
    -   `app/public` is for assets that the app may want to access, like images, svgs, etc.
    -   `app/dist` will be generated when doing a build, these are the .html,.js, and .css files for production.

### tailwindcss

Styling for the app is done with [tailwindcss](https://tailwindcss.com/).

-   Installation
    -   Running `bun install` within this directory will install it as a dev dependency.
    -   It can be used with `bunx`.
    -   Or as a standalone executable. Get the binary for your OS [here](https://github.com/tailwindlabs/tailwindcss/releases)
-   Usage
    -   Be sure to run tailwind in the same directory as the 'tailwind.config.js'.
    -   To use while developing, you will likely want to watch for css changes, here is an example:  
        `bunx tailwindcss -i src/assets/input.css -o src/assets/output.css --watch`
-   Optimising for production
    -   To minify the 'output.css', use this:  
        `bunx tailwindcss -o src/assets/output.css --minify`

### Linting & Formatting

Keeping consistent styling is important, please run both the linter and formatter before submitting any changes to see potential problems.  
Check in the package.json to see how to run the relevant linters and formatters.  
The linter being used is [TSLint](https://typescript-eslint.io/getting-started). The formatter is [Prettier](https://prettier.io/docs/en/).  
Rules can be changed in their relevant .rc files.

When running `bun run lint`, you may notice an output about running prettier, go ahead and run `bun run format` so prettier can format the files.  
Then, run lint again. Another thing to note is that you may get some errors regarding the use of type `any`.  
We want to avoid using type `any` at all cost, but sometimes giving a type to very arbitrary values is tough and tedious, use it as a last resort.

## Building

Be sure to have everything installed: `bun install`.  
Then, `bun run build` to have Vite build and output files into the `dist` directory.  
Those files will be the ones used in release.

### Minifying

When doing a release build, it is useful to optimise the code further, whether it be for speed or size.  
'Minify' means to make the files as small as possible. Lets see how we can minify the output.

First, lets minify the css:  
`bunx tailwindcss -o src/assets/output.css --minify`  
Then, the html and js:  
`bun run minify` - Check the package.json to see what that does.  
You could run just the second step. But maybe tailwind makes different decisions or optimisation on minifying its css.

We can see the result is incredibly small files:

```bash
dist/index.html                  0.60 kB │ gzip:  0.37 kB
dist/assets/index-CiR6En4v.css  12.07 kB │ gzip:  3.05 kB
dist/assets/index-DnwvALoA.js   49.59 kB │ gzip: 16.28 kB
✓ built in 456ms
```
