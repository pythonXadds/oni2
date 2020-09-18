open Oni_Core;

// MODEL

[@deriving show]
type msg;

module Msg: {
  let keyPressed: string => msg;
  let activeFileChanged: option(string) => msg;
};

type model;

let initial: (~rootPath: string) => model;

let setRoot: (~rootPath: string, model) => model;

let getFileIcon:
  (~languageInfo: Exthost.LanguageInfo.t, ~iconTheme: IconTheme.t, string) =>
  option(IconTheme.IconDefinition.t);

// UPDATE

type outmsg =
  | Nothing
  | Effect(Isolinear.Effect.t(msg))
  | OpenFile(string)
  | GrabFocus;

let update:
  (
    ~configuration: Oni_Core.Configuration.t,
    ~languageInfo: Exthost.LanguageInfo.t,
    ~iconTheme: Oni_Core.IconTheme.t,
    msg,
    model
  ) =>
  (model, outmsg);

// SUBSCRIPTION

let sub:
  (
    ~configuration: Oni_Core.Configuration.t,
    ~languageInfo: Exthost.LanguageInfo.t,
    ~iconTheme: Oni_Core.IconTheme.t,
    model
  ) =>
  Isolinear.Sub.t(msg);

// VIEW

module View: {
  let make:
    (
      ~model: model,
      ~decorations: Feature_Decorations.model,
      ~theme: ColorTheme.Colors.t,
      ~font: UiFont.t,
      ~dispatch: msg => unit,
      unit
    ) =>
    Revery.UI.element;
};