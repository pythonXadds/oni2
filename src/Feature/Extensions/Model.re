open Oni_Core;
open Exthost.Extension;

[@deriving show({with_path: false})]
type msg =
  | Exthost(Exthost.Msg.ExtensionService.msg)
  | Storage({
      resolver: [@opaque] Lwt.u(Exthost.Reply.t),
      msg: Exthost.Msg.Storage.msg,
    })
  | Discovered([@opaque] list(Scanner.ScanResult.t))
  | ExecuteCommand({
      command: string,
      arguments: [@opaque] list(Json.t),
    })
  | KeyPressed(string)
  | Pasted(string)
  | SearchQueryResults(Service_Extensions.Query.t)
  | SearchQueryError(string)
  | SearchText(Component_InputText.msg)
  | UninstallExtensionClicked({extensionId: string})
  | UninstallExtensionSuccess({extensionId: string})
  | UninstallExtensionFailed({
      extensionId: string,
      errorMsg: string,
    })
  | InstallExtensionClicked({extensionId: string})
  | InstallExtensionSuccess({
      extensionId: string,
      scanResult: [@opaque] Scanner.ScanResult.t,
    })
  | InstallExtensionFailed({
      extensionId: string,
      errorMsg: string,
    })
  | SetThemeClicked({extensionId: string})
  | LocalExtensionSelected({extensionInfo: [@opaque] Scanner.ScanResult.t})
  | RemoteExtensionClicked({extensionId: string})
  | RemoteExtensionSelected({
      extensionInfo: Service_Extensions.Catalog.Details.t,
    })
  | RemoteExtensionUnableToFetchDetails({errorMsg: string})
  | VimWindowNav(Component_VimWindows.msg);

module Msg = {
  let exthost = msg => Exthost(msg);
  let keyPressed = key => KeyPressed(key);
  let pasted = contents => Pasted(contents);
  let discovered = scanResult => Discovered(scanResult);
};

type outmsg =
  | Nothing
  | Focus
  | Effect(Isolinear.Effect.t(msg))
  | InstallSucceeded({
      extensionId: string,
      contributions: Exthost.Extension.Contributions.t,
    })
  | NotifySuccess(string)
  | NotifyFailure(string)
  | OpenExtensionDetails
  | SelectTheme({themes: list(Exthost.Extension.Contributions.Theme.t)})
  | UnhandledWindowMovement(Component_VimWindows.outmsg);

module Selected = {
  open Exthost_Extension;
  open Exthost_Extension.Manifest;
  type t =
    | Local(Scanner.ScanResult.t)
    | Remote(Service_Extensions.Catalog.Details.t);

  let identifier =
    fun
    | Local(scanResult) => scanResult.manifest |> Manifest.identifier
    | Remote(details) => details.namespace ++ "." ++ details.name;

  let logo =
    fun
    | Local(scanResult) => scanResult.manifest.icon
    | Remote({iconUrl, _}) => iconUrl;

  let displayName =
    fun
    | Local(scanResult) => scanResult.manifest |> Manifest.getDisplayName
    | Remote({displayName, namespace, name, _}) =>
      displayName |> Option.value(~default=namespace ++ "." ++ name);

  let description =
    fun
    | Local(scanResult) => scanResult.manifest.description
    | Remote({description, _}) => Some(description);

  let readme =
    fun
    | Local(scanResult) =>
      Some(Rench.Path.join(scanResult.path, "README.md"))
    | Remote({readmeUrl, _}) => readmeUrl;

  let version =
    fun
    | Local({manifest, _}) => manifest.version
    | Remote({version, _}) => version;
};

module Effect = {
  let replySuccess = (~resolver) =>
    Isolinear.Effect.create(~name="feature.extensions.replySuccess", () => {
      Lwt.wakeup(resolver, Exthost.Reply.okEmpty)
    });

  let replyJson = (~resolver, json) =>
    Isolinear.Effect.create(~name="feature.extensions.replyJson", () => {
      Lwt.wakeup(resolver, Exthost.Reply.okJson(json))
    });
};

type extensionState = {isRestartRequired: bool};

module Focus = {
  type t =
    | SearchText
    | Installed
    | Bundled;

  let initial = SearchText;

  let moveDown = (~isSearching, focus) => {
    switch (focus) {
    | SearchText => Some(Installed)
    | Installed when isSearching => None
    | Installed => Some(Bundled)
    | Bundled => None
    };
  };

  let moveUp = (~isSearching, focus) => {
    switch (focus) {
    | SearchText => None
    | Installed => Some(SearchText)
    | Bundled when isSearching => None
    | Bundled => Some(Installed)
    };
  };
};

type model = {
  selected: option(Selected.t),
  activatedIds: list(string),
  extensions: list(Scanner.ScanResult.t),
  searchText: Component_InputText.model,
  latestQuery: option(Service_Extensions.Query.t),
  extensionsFolder: option(string),
  pendingInstalls: list(string),
  pendingUninstalls: list(string),
  globalValues: Yojson.Safe.t,
  localValues: Yojson.Safe.t,
  extensionState: StringMap.t(extensionState),
  focusedWindow: Focus.t,
  vimWindowNavigation: Component_VimWindows.model,
};

module Persistence = {
  type t = Yojson.Safe.t;
  let initial = `Assoc([]);

  let codec = Oni_Core.Persistence.Schema.value;

  let get = (~shared, model) =>
    shared ? model.globalValues : model.localValues;
};

let initial = (~workspacePersistence, ~globalPersistence, ~extensionsFolder) => {
  activatedIds: [],
  selected: None,
  extensions: [],
  searchText: Component_InputText.create(~placeholder="Type to search..."),
  latestQuery: None,
  extensionsFolder,
  pendingInstalls: [],
  pendingUninstalls: [],
  globalValues: globalPersistence,
  localValues: workspacePersistence,
  extensionState: StringMap.empty,

  focusedWindow: Focus.initial,
  vimWindowNavigation: Component_VimWindows.initial,
};

let isBusy = ({pendingInstalls, pendingUninstalls, _}) => {
  pendingInstalls != [] || pendingUninstalls != [];
};

let isInstalling = (~extensionId, {pendingInstalls, _}) => {
  pendingInstalls |> List.exists(id => id == extensionId);
};

let themes = (~extensionId, {extensions, _}) => {
  extensions
  |> List.filter((ext: Exthost.Extension.Scanner.ScanResult.t) =>
       String.equal(ext.manifest |> Manifest.identifier, extensionId)
     )
  |> (
    l =>
      List.nth_opt(l, 0)
      |> Option.map(({manifest, _}: Exthost.Extension.Scanner.ScanResult.t) =>
           manifest.contributes.themes
         )
  );
};

let isInstalled = (~extensionId, {extensions, _}) => {
  extensions
  |> List.filter((ext: Exthost.Extension.Scanner.ScanResult.t) =>
       String.equal(ext.manifest |> Manifest.identifier, extensionId)
     )
  != [];
};

let hasThemes = (~extensionId, model) => {
  themes(~extensionId, model)
  |> Option.map(themes => themes != [])
  |> Option.value(~default=false);
};

let isUninstalling = (~extensionId, {pendingUninstalls, _}) => {
  pendingUninstalls |> List.exists(id => id == extensionId);
};

let isRestartRequired = (~extensionId, {extensionState, _}) => {
  extensionState
  |> StringMap.find_opt(extensionId)
  |> Option.map(({isRestartRequired}) => isRestartRequired)
  |> Option.value(~default=false);
};

let searchResults = ({latestQuery, _}) =>
  switch (latestQuery) {
  | None => []
  | Some(query) => query |> Service_Extensions.Query.results
  };

let isSearchInProgress = ({latestQuery, _}) => {
  switch (latestQuery) {
  | None => false
  | Some(query) => !(query |> Service_Extensions.Query.isComplete)
  };
};

module Internal = {
  let filterBundled = (scanner: Scanner.ScanResult.t) => {
    let name = scanner.manifest |> Manifest.identifier;

    name == "vscode.typescript-language-features"
    || name == "vscode.css-language-features"
    || name == "jaredkent.laserwave"
    || name == "jaredly.reason-vscode"
    || name == "arcticicestudio.nord-visual-studio-code";
  };
  let markActivated = (id: string, model) => {
    ...model,
    activatedIds: [id, ...model.activatedIds],
  };

  let add = (extensions, model) => {
    ...model,
    extensions: extensions @ model.extensions,
  };

  let addPendingInstall = (~extensionId, model) => {
    ...model,
    pendingInstalls: [extensionId, ...model.pendingInstalls],
  };

  let addPendingUninstall = (~extensionId, model) => {
    ...model,
    pendingUninstalls: [extensionId, ...model.pendingUninstalls],
  };

  let clearPendingInstall = (~extensionId, model) => {
    ...model,
    pendingInstalls:
      List.filter(id => id != extensionId, model.pendingInstalls),
  };

  let clearPendingUninstall = (~extensionId, model) => {
    ...model,
    pendingUninstalls:
      List.filter(id => id != extensionId, model.pendingUninstalls),
  };

  let markRestartNeeded = (~extensionId, model) => {
    ...model,
    extensionState:
      StringMap.update(
        extensionId,
        fun
        | None => Some({isRestartRequired: true})
        | Some(_prev) => Some({isRestartRequired: true}),
        model.extensionState,
      ),
  };

  let installed = (~extensionId, ~scanResult, model) => {
    let model' =
      model
      |> clearPendingInstall(~extensionId)
      |> markRestartNeeded(~extensionId);

    {...model', extensions: [scanResult, ...model'.extensions]};
  };

  let uninstalled = (~extensionId, model) => {
    let model' = model |> clearPendingUninstall(~extensionId);

    {
      ...model',
      extensions:
        List.filter(
          (scanResult: Exthost.Extension.Scanner.ScanResult.t) => {
            scanResult.manifest
            |> Exthost.Extension.Manifest.identifier != extensionId
          },
          model'.extensions,
        ),
    };
  };
};

let getExtensions = (~category, model) => {
  let results =
    model.extensions
    |> List.filter((ext: Scanner.ScanResult.t) => ext.category == category);

  switch (category) {
  | Scanner.Bundled => List.filter(Internal.filterBundled, results)
  | _ => results
  };
};

let getPersistedValue = (~shared, ~key, model) => {
  let store = shared ? model.globalValues : model.localValues;
  [store]
  |> Yojson.Safe.Util.filter_member(key)
  |> (l => List.nth_opt(l, 0));
};

let checkAndUpdateSearchText = (~previousText, ~newText, ~query) =>
  if (previousText != newText) {
    if (String.length(newText) == 0) {
      None;
    } else {
      Some(Service_Extensions.Query.create(~searchText=newText));
    };
  } else {
    query;
  };

let update = (~extHostClient, msg, model) => {
  switch (msg) {
  | Exthost(WillActivateExtension(_))
  | Exthost(ExtensionRuntimeError(_)) => (model, Nothing)
  | Exthost(ActivateExtension({extensionId, _})) => (
      Internal.markActivated(extensionId, model),
      Nothing,
    )
  | Exthost(ExtensionActivationError({errorMessage, _})) => (
      model,
      NotifyFailure(Printf.sprintf("Error: %s", errorMessage)),
    )
  | Exthost(DidActivateExtension({extensionId, _})) => (
      Internal.markActivated(extensionId, model),
      Nothing,
    )

  | Storage({resolver, msg}) =>
    switch (msg) {
    | GetValue({shared, key}) =>
      let value = getPersistedValue(~shared, ~key, model);

      let eff =
        switch (value) {
        | None => Effect.replyJson(~resolver, `Null)
        | Some(json) => Effect.replyJson(~resolver, json)
        };
      (model, Effect(eff));

    | SetValue({shared, key, value}) =>
      let store = shared ? model.globalValues : model.localValues;
      let store' = Utility.JsonEx.update(key, _ => Some(value), store);
      let model' =
        shared
          ? {...model, globalValues: store'}
          : {...model, localValues: store'};

      let eff = Effect.replySuccess(~resolver);
      (model', Effect(eff));
    }

  | Discovered(extensions) => (Internal.add(extensions, model), Nothing)

  | ExecuteCommand({command, arguments}) => (
      model,
      Effect(
        Service_Exthost.Effects.Commands.executeContributedCommand(
          ~command,
          ~arguments,
          extHostClient,
        ),
      ),
    )

  | KeyPressed(key) =>
    let previousText = model.searchText |> Component_InputText.value;
    let searchText' = Component_InputText.handleInput(~key, model.searchText);
    let newText = searchText' |> Component_InputText.value;
    let latestQuery =
      checkAndUpdateSearchText(
        ~previousText,
        ~newText,
        ~query=model.latestQuery,
      );
    ({...model, searchText: searchText', latestQuery}, Nothing);
  | Pasted(text) =>
    let previousText = model.searchText |> Component_InputText.value;
    let searchText' = Component_InputText.paste(~text, model.searchText);
    let newText = searchText' |> Component_InputText.value;
    let latestQuery =
      checkAndUpdateSearchText(
        ~previousText,
        ~newText,
        ~query=model.latestQuery,
      );
    ({...model, searchText: searchText', latestQuery}, Nothing);
  | SearchText(msg) =>
    let previousText = model.searchText |> Component_InputText.value;
    let (searchText', inputOutmsg) =
      Component_InputText.update(msg, model.searchText);
    let outmsg =
      switch (inputOutmsg) {
      | Component_InputText.Nothing => Nothing
      | Component_InputText.Focus => Focus
      };
    let newText = searchText' |> Component_InputText.value;
    let latestQuery =
      checkAndUpdateSearchText(
        ~previousText,
        ~newText,
        ~query=model.latestQuery,
      );
    ({...model, searchText: searchText', latestQuery}, outmsg);
  | SearchQueryResults(queryResults) =>
    queryResults
    |> Service_Extensions.Query.searchText
    == (model.searchText |> Component_InputText.value)
      ? ({...model, latestQuery: Some(queryResults)}, Nothing)
      : (model, Nothing)
  | SearchQueryError(_queryResults) =>
    // TODO: Error experience?
    ({...model, latestQuery: None}, Nothing)
  | UninstallExtensionClicked({extensionId}) =>
    let toMsg = (
      fun
      | Ok () => UninstallExtensionSuccess({extensionId: extensionId})
      | Error(msg) => UninstallExtensionFailed({extensionId, errorMsg: msg})
    );
    let eff =
      Service_Extensions.Effects.uninstall(
        ~extensionsFolder=model.extensionsFolder,
        ~toMsg,
        extensionId,
      );
    (model |> Internal.addPendingUninstall(~extensionId), Effect(eff));
  | UninstallExtensionSuccess({extensionId}) => (
      model |> Internal.uninstalled(~extensionId),
      NotifySuccess(
        Printf.sprintf("Successfully uninstalled %s", extensionId),
      ),
    )
  | UninstallExtensionFailed({extensionId, errorMsg}) => (
      model |> Internal.clearPendingUninstall(~extensionId),
      NotifyFailure(
        Printf.sprintf(
          "Extension %s failed to uninstall: %s",
          extensionId,
          errorMsg,
        ),
      ),
    )
  | InstallExtensionClicked({extensionId}) =>
    let toMsg = (
      fun
      | Ok(scanResult) => InstallExtensionSuccess({extensionId, scanResult})
      | Error(msg) => InstallExtensionFailed({extensionId, errorMsg: msg})
    );
    let eff =
      Service_Extensions.Effects.install(
        ~extensionsFolder=model.extensionsFolder,
        ~toMsg,
        extensionId,
      );
    (model |> Internal.addPendingInstall(~extensionId), Effect(eff));

  | InstallExtensionSuccess({extensionId, scanResult}) => (
      model |> Internal.installed(~extensionId, ~scanResult),
      InstallSucceeded({
        extensionId,
        contributions:
          Exthost.Extension.Manifest.(scanResult.manifest.contributes),
      }),
    )
  | InstallExtensionFailed({extensionId, errorMsg}) => (
      model |> Internal.clearPendingInstall(~extensionId),
      NotifyFailure(
        Printf.sprintf(
          "Extension %s failed to install: %s",
          extensionId,
          errorMsg,
        ),
      ),
    )
  | LocalExtensionSelected({extensionInfo}) => (
      {...model, selected: Some(Selected.Local(extensionInfo))},
      OpenExtensionDetails,
    )
  | RemoteExtensionClicked({extensionId}) => (
      model,
      Effect(
        Service_Extensions.Effects.details(
          ~extensionId,
          ~toMsg={
            fun
            | Ok(extensionInfo) =>
              RemoteExtensionSelected({extensionInfo: extensionInfo})
            | Error(errorMsg) =>
              RemoteExtensionUnableToFetchDetails({errorMsg: errorMsg});
          },
        ),
      ),
    )
  | RemoteExtensionSelected({extensionInfo}) => (
      {...model, selected: Some(Selected.Remote(extensionInfo))},
      OpenExtensionDetails,
    )
  | RemoteExtensionUnableToFetchDetails({errorMsg}) => (
      model,
      NotifyFailure(errorMsg),
    )

  | SetThemeClicked({extensionId}) =>
    themes(~extensionId, model)
    |> Option.map(themes => {(model, SelectTheme({themes: themes}))})
    |> Option.value(
         ~default=(
           model,
           NotifyFailure(
             Printf.sprintf("Unable to find extension: %s", extensionId),
           ),
         ),
       )

  | VimWindowNav(navMsg) =>
    let (windowNav, outmsg) =
      Component_VimWindows.update(navMsg, model.vimWindowNavigation);

    let model' = {...model, vimWindowNavigation: windowNav};
    let isSearching = !Component_InputText.isEmpty(model.searchText);
    let focusedWindow = model'.focusedWindow;

    let (focus, outmsg) =
      switch (outmsg) {
      | Nothing => (focusedWindow, Nothing)
      | FocusLeft => (focusedWindow, UnhandledWindowMovement(outmsg))
      | FocusRight => (focusedWindow, UnhandledWindowMovement(outmsg))
      | FocusDown =>
        switch (Focus.moveDown(~isSearching, focusedWindow)) {
        | None => (focusedWindow, UnhandledWindowMovement(outmsg))
        | Some(focus) => (focus, Nothing)
        }
      | FocusUp =>
        switch (Focus.moveUp(~isSearching, focusedWindow)) {
        | None => (focusedWindow, UnhandledWindowMovement(outmsg))
        | Some(focus) => (focus, Nothing)
        }

      // TODO
      | PreviousTab
      | NextTab => (focusedWindow, Nothing)
      };
    ({...model', focusedWindow: focus}, outmsg);
  };
};
