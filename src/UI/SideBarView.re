open Revery.UI;

open Oni_Model;
module Core = Oni_Core;

open Feature_SideBar;
open Oni_Components;

module Colors = Feature_Theme.Colors;

module Styles = {
  open Style;

  let sidebar = (~theme, ~transition) => [
    flexDirection(`Row),
    backgroundColor(Colors.SideBar.background.from(theme)),
    transform(Transform.[TranslateX(transition)]),
  ];

  let contents = (~width) => [
    Style.width(width),
    flexDirection(`Column),
    flexGrow(1),
  ];

  let resizer = [flexGrow(0), width(4), position(`Relative)];

  let title = (~theme) => [color(Colors.SideBar.foreground.from(theme))];

  let titleContainer = [paddingLeft(23)];

  let heading = theme => [
    flexDirection(`Row),
    alignItems(`Center),
    backgroundColor(Colors.SideBar.background.from(theme)),
    height(Core.Constants.tabHeight),
  ];

  let separator = [
    width(4),
    backgroundColor(Revery.Color.rgba(0., 0., 0., 0.1)),
  ];
};

let separator = () => {
  <View style=Styles.separator />;
};

let animation =
  Revery.UI.Animation.(
    animate(Revery.Time.milliseconds(150))
    |> ease(Easing.easeIn)
    |> tween(-50.0, 0.)
    |> delay(Revery.Time.milliseconds(0))
  );

let%component make = (~theme, ~state: State.t, ~dispatch, ()) => {
  let State.{sideBar, uiFont: font, _} = state;

  let%hook (transition, _animationState, _reset) =
    Hooks.animation(animation, ~active=true);

  let title =
    switch (sideBar |> selected) {
    | FileExplorer => "Explorer"
    | SCM => "Source Control"
    | Extensions => "Extensions"
    | Search => "Search"
    };

  let elem =
    switch (sideBar |> selected) {
    | FileExplorer =>
      let dispatch = msg => dispatch(Actions.FileExplorer(msg));
      <Feature_Explorer.View
        decorations={state.decorations}
        model={state.fileExplorer}
        theme
        font
        dispatch
      />;

    | SCM =>
      let onItemClick = (resource: Feature_SCM.Resource.t) =>
        dispatch(
          Actions.OpenFileByPath(
            Oni_Core.Uri.toFileSystemPath(resource.uri),
            None,
            None,
          ),
        );

      <Feature_SCM.Pane
        model={state.scm}
        workingDirectory={state.workspace.workingDirectory}
        onItemClick
        isFocused={FocusManager.current(state) == Focus.SCM}
        theme
        font
        dispatch={msg => dispatch(Actions.SCM(msg))}
      />;

    | Search =>
      let dispatch = msg =>
        GlobalContext.current().dispatch(Actions.Search(msg));

      <Feature_Search
        isFocused={FocusManager.current(state) == Focus.Search}
        theme
        uiFont={state.uiFont}
        editorFont={state.editorFont}
        model={state.searchPane}
        dispatch
        workingDirectory={state.workspace.workingDirectory}
      />;

    | Extensions =>
      let extensionDispatch = msg => dispatch(Actions.Extensions(msg));
      <Feature_Extensions.ListView
        model={state.extensions}
        theme
        font
        isFocused={FocusManager.current(state) == Focus.Extensions}
        dispatch=extensionDispatch
      />;
    };

  let width = Feature_SideBar.width(state.sideBar);
  let separator =
    Feature_SideBar.isOpen(state.sideBar) && width > 4
      ? <separator /> : React.empty;

  let content =
    <View style={Styles.contents(~width)}>
      <View style={Styles.heading(theme)}>
        <View style=Styles.titleContainer>
          <Text
            text=title
            style={Styles.title(~theme)}
            fontFamily={font.family}
            fontWeight=Revery.Font.Weight.SemiBold
            fontSize=13.
          />
        </View>
      </View>
      elem
    </View>;

  let resizer =
    <View style=Styles.resizer>
      <ResizeHandle.Vertical
        onDrag={delta =>
          dispatch(
            Actions.SideBar(
              Feature_SideBar.ResizeInProgress(int_of_float(delta)),
            ),
          )
        }
        onDragComplete={() =>
          dispatch(Actions.SideBar(Feature_SideBar.ResizeCommitted))
        }
      />
    </View>;

  let defaultOrder = [content, resizer];
  let items =
    switch (Feature_SideBar.location(sideBar)) {
    | Feature_SideBar.Left => defaultOrder
    | Feature_SideBar.Right => List.rev(defaultOrder)
    };

  <View style={Styles.sidebar(~theme, ~transition)}>
    separator
    {React.listToElement(items)}
  </View>;
};
