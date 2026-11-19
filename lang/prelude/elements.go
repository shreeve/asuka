// Package prelude is what every Asuka program sees without importing
// it: the core types (types.asuka) and the built-in elements, declared
// here once. go generate writes, from this table:
//
//   - elements.asuka: the elements as Asuka declarations (native
//     components), which the checker reads like any source, and which
//     editors show and jump to;
//   - core/src/ui/elements_gen.h: the C UI runtime's constants (AK_EL_,
//     AK_KEY_, AK_EV_, AK_SLOT_) and tables.
//
// Adding an element: its entry here, go generate ./lang/prelude, its
// behavior in core/src/ui (and its native view in every host, when it
// makes a new kind of view: ops/schema.go).
package prelude

//go:generate go run ./gen

// Key is a prop of an element: set as name: value in its block, or
// passed in its parentheses when Positional.
type Key struct {
	Name string
	// Type is an Asuka type (types.asuka's or a built-in one).
	Type string
	// Positional keys are the element's header parameters, in order:
	// Text("Hi"). The rest are named props.
	Positional bool
	// Default is the Asuka source of the default; "" means the type's
	// zero value (or, for an optional, none).
	Default string
	// Bind marks a key a control writes back: bind text: query.
	Bind bool
	// Event is, for a Bind key, the event the control writes it with.
	Event string
	// Builder: a fn returning a View the runtime calls with accessors (a
	// List's header, of its section's key, which a recycled header
	// follows): its lambda's parameters read as a for's row item.
	Builder bool
	Doc     string
}

// Param is an event's parameter.
type Param struct {
	Name string
	Type string
}

// Event is what an element reports: on press { }.
type Event struct {
	Name   string
	Params []Param
	// Builds: the handler's block is views, and the handler returns the
	// View it builds (a Boundary's error: what shows instead).
	Builds bool
	Doc    string
}

// Slot is a named place for views: toolbar { }.
type Slot struct {
	Name string
	// Lazy: the slot's views are passed as a builder (ui.slot with a
	// closure returning a View), built only when they show (a Popover's
	// content, as it opens).
	Lazy bool
	Doc  string
}

// Element is a built-in element.
type Element struct {
	Name string
	// Children: the element takes children in its block.
	Children bool
	// LazyFor: a for directly in its block builds its rows only while
	// they're on screen (a List).
	LazyFor bool
	// LazyChildren: its children are passed as a builder (ui.lazy(v,
	// builder)), built only when they show and disposed when they go: a
	// Tab's page as it's first chosen, a Sheet's content as it opens.
	// Params are what the builder is called with (a Route's: its
	// parameters, an accessor).
	LazyChildren bool
	Params       []Param
	// NoStyle: the element takes none of the style keys (a fragment).
	NoStyle bool
	// Variants: the element takes the style's variants, blocks of style
	// keys laid over the rest while it's in an interaction state
	// (Variants below): pressed { opacity: 0.6 }.
	Variants bool
	Keys     []Key
	Events   []Event
	Slots    []Slot
	Doc      string
}

// Style are the keys every element takes (unless NoStyle): layout, then
// look. They're fields of the element's style, as the Go ui's Style.
var Style = []Key{
	// Layout.
	{Name: "direction", Type: "Axis", Doc: "the axis children stack on: .column (the default) or .row"},
	{Name: "wrap", Type: "bool", Doc: "children wrap onto more lines"},
	{Name: "justify", Type: "Justify", Doc: "children's placement on the main axis"},
	{Name: "align", Type: "Align", Doc: "children's alignment on the cross axis"},
	{Name: "alignSelf", Type: "Align", Doc: "this element's alignment in its parent, over the parent's align"},
	{Name: "gap", Type: "float", Doc: "space between children, and between lines"},
	{Name: "padding", Type: "Edges", Doc: "space inside the border: 16, [8, 16] (vertical, horizontal), [t, r, b, l] or Edges(top: 4)"},
	{Name: "margin", Type: "Edges", Doc: "space outside the border"},
	{Name: "width", Type: "Length", Doc: "100, 50% or .auto (the default)"},
	{Name: "height", Type: "Length"},
	{Name: "minWidth", Type: "Length"},
	{Name: "minHeight", Type: "Length"},
	{Name: "maxWidth", Type: "Length"},
	{Name: "maxHeight", Type: "Length"},
	{Name: "aspectRatio", Type: "float", Doc: "width / height"},
	{Name: "grow", Type: "float", Doc: "its share of the space left on its parent's main axis"},
	{Name: "shrink", Type: "float", Doc: "how much it gives up when the line overflows (0: the default)"},
	{Name: "basis", Type: "Length", Doc: "its starting size on the main axis (.auto: its content's)"},
	{Name: "absolute", Type: "bool", Doc: "out of the flow, placed by top, right, bottom and left in its parent"},
	{Name: "top", Type: "Length"},
	{Name: "right", Type: "Length"},
	{Name: "bottom", Type: "Length"},
	{Name: "left", Type: "Length"},
	{Name: "start", Type: "Length", Doc: "from the side a line of text starts on (left, or right in Arabic)"},
	{Name: "end", Type: "Length"},
	{Name: "grid", Type: "Grid?", Doc: "lays children out on a grid"},
	{Name: "gridColumn", Type: "GridLine"},
	{Name: "gridRow", Type: "GridLine"},
	// Look.
	{Name: "background", Type: "Color"},
	{Name: "radius", Type: "float", Doc: "corner radius"},
	{Name: "border", Type: "float", Doc: "border width, drawn inside the box"},
	{Name: "borderColor", Type: "Color"},
	{Name: "opacity", Type: "float", Default: "1"},
	{Name: "hidden", Type: "bool"},
	{Name: "clip", Type: "bool", Doc: "clips children to its bounds"},
	{Name: "transform", Type: "Transform", Doc: "moves, scales and rotates it as it's drawn; layout doesn't see it"},
	{Name: "shadow", Type: "Shadow?"},
	{Name: "gradient", Type: "Gradient?"},
	{Name: "material", Type: "Material", Doc: "a translucent background that blurs what's behind"},
	{Name: "transition", Type: "Transition", Doc: "how it comes in and goes out during animate { }"},
	{Name: "shared", Type: "string", Doc: "a key it shares with another view: during animate { }, the one that comes moves from where the one that goes was"},
	{Name: "font", Type: "Font"},
	{Name: "color", Type: "Color", Doc: "text color, or a control's tint"},
	{Name: "textAlign", Type: "TextAlign"},
	{Name: "lines", Type: "int", Doc: "the most lines of text (0: no limit)"},
	// Accessibility.
	{Name: "label", Type: "string", Doc: "what screen readers read"},
	{Name: "hint", Type: "string"},
	{Name: "role", Type: "Role"},
	{Name: "a11yHidden", Type: "bool", Doc: "hidden from screen readers, with what's in it"},
	{Name: "a11yChecked", Type: "Checked", Doc: "a checkbox's, switch's, radio's or checkable menu item's state, for screen readers: .unchecked, .checked, .mixed (.off: not checkable; read as unchecked with a checkable role)"},
	{Name: "a11yExpanded", Type: "Expanded", Doc: "whether what it controls shows (a disclosure, a menu's trigger, a combobox): .collapsed or .expanded (.off: it expands nothing)"},
	{Name: "a11ySelected", Type: "bool", Doc: "selected among its group, for screen readers: a tab, an option, a radio group's choice"},
	{Name: "a11yValue", Type: "string", Doc: "what screen readers read as its value: a slider's \"40%\", a select's choice"},
	{Name: "a11yRange", Type: "ValueRange?", Doc: "a slider's, progress bar's, meter's or spin button's range, for screen readers"},
	{Name: "a11yModal", Type: "bool", Doc: "screen readers stay inside it while it shows, the rest of its window hidden from them: a dialog"},
	{Name: "a11yControls", Type: "Ref?", Doc: "the view this one controls: a combobox's listbox, a tab's panel (Windows, Linux and the web have the relation)"},
	{Name: "a11yLabelledBy", Type: "Ref?", Doc: "the view whose text labels this one: a field's label"},
	{Name: "a11yDescribedBy", Type: "Ref?", Doc: "the view whose text describes this one: a field's description or error"},
	{Name: "tooltip", Type: "string"},
	{Name: "disabled", Type: "bool"},
	// Direction.
	{Name: "layoutDirection", Type: "LayoutDirection", Doc: "lays it and what's in it out this way, whatever the app's language: what reads one way everywhere (a phone number, a player's controls)"},
	// Keys and screen readers' actions.
	{Name: "keys", Type: "[Shortcut]", Doc: "what it does with keys while the keyboard's focus is in it (it, or a view inside it), before the focused control or the menu bar: the nearest view out from the focused one with the key takes it"},
	{Name: "actions", Type: "[A11yAction]", Doc: "what a screen reader offers to do with it (VoiceOver's rotor, TalkBack's actions menu)"},
}

// Variants are the interaction states a style variant follows, in the
// order they're laid over the style (a later one wins): a block of
// style keys named after the state, in the block of an element whose
// Variants is set. The element reports a state once a variant (or
// ui.state) asks for it.
var Variants = []string{"hovered", "pressed", "dragging", "dragOver", "focused", "focusVisible"}

// containerKeys are what View, Row, Column and SafeArea take besides the
// style: drags and drops.
var containerKeys = []Key{
	{Name: "dragItems", Type: "[DragItem]", Doc: "makes it draggable, within the app and to other apps: a long press then a move lifts it on a touch screen, a move with the button down with a mouse. [] (the default): not draggable"},
	{Name: "dropTypes", Type: "[string]", Doc: "makes it take drops of these MIME types: exact (\"text/plain\"), a family (\"image/*\"), anything (\"*/*\"), or the app's own"},
	{Name: "swipeActions", Type: "[Action]", Doc: "a List's row's actions, revealed by swiping it from its trailing edge (a destructive one red; the first a full swipe's): on the row's top view"},
	{Name: "leadingSwipeActions", Type: "[Action]", Doc: "a List's row's actions revealed from its leading edge (Mail's Unread)"},
	{Name: "selected", Type: "bool", Doc: "a List's row is selected: in a selecting list, its selection control is checked (what on select doesn't take back goes back as it was)"},
	{Name: "href", Type: "string", Doc: "goes to this route when it's pressed, after press (the Router around it navigates there): an href, or a routed screen's call; with role: .link, screen readers call it a link"},
	{Name: "focusable", Type: "bool", Doc: "takes the keyboard's focus: a Tab stop in the views' order where a keyboard moves focus (desktops, the web, a hardware keyboard on a phone or tablet), focused by a press with a mouse. A pressable or adjustable view is one already"},
	{Name: "pressKeepsFocus", Type: "bool", Doc: "a press with a pointer leaves the keyboard's focus where it was (a toolbar's button, a menu's trigger)"},
	{Name: "hideFocusRing", Type: "bool", Doc: "the platform draws no focus ring around it: the app draws its own (a view that styles focusVisible { } hides it already)"},
	{Name: "trapFocus", Type: "bool", Doc: "while the keyboard's focus is in it, Tab and Shift+Tab cycle among the focusable views inside it: a dialog's"},
	{Name: "restoreFocus", Type: "bool", Doc: "when it goes, the keyboard's focus goes back to the view that had it when it came, if that view still shows"},
	{Name: "autofocus", Type: "bool", Doc: "once it's laid out, the keyboard's focus goes to it if it's focusable, else to the first focusable view inside it (a field, a pressable view)"},
}

// containerEvents are what View, Row, Column and SafeArea report: a
// container with any of them has a native view of its own.
var containerEvents = []Event{
	{Name: "press", Doc: "a tap on it"},
	{Name: "longPress", Doc: "a press held on it"},
	{Name: "layout", Params: []Param{{"frame", "Frame"}}, Doc: "its frame once it's laid out, and again each time layout changes it"},
	{Name: "pressedChange", Params: []Param{{"pressed", "bool"}}, Doc: "a finger, or the mouse's primary button, down on it (CSS's :active), and up, off it, or taken by a scroll or a gesture; it then draws its own pressed look, not the platform's"},
	{Name: "hoverChange", Params: []Param{{"hovered", "bool"}}, Doc: "a pointer over it or its children (CSS's :hover), and off it; a touch screen alone never hovers"},
	{Name: "pan", Params: []Param{{"gesture", "Gesture"}}, Doc: "a drag across it, as it goes"},
	{Name: "pinch", Params: []Param{{"gesture", "Gesture"}}},
	{Name: "rotate", Params: []Param{{"gesture", "Gesture"}}, Doc: "two fingers turning: the gesture's rotation, in radians clockwise"},
	{Name: "swipe", Params: []Param{{"direction", "SwipeDirection"}}},
	{Name: "adjust", Params: []Param{{"by", "int"}}, Doc: "makes it adjustable, as a slider is: a screen reader's swipe up or down steps it, by 1 or -1"},
	{Name: "dragPhase", Params: []Param{{"phase", "DragPhase"}}, Doc: "its drag: began as it lifts, then dropped (a target took it: for a move, remove what was dragged) or cancelled"},
	{Name: "dropOver", Params: []Param{{"over", "bool"}}, Doc: "a drag it takes coming over it, and leaving it (a drop zone's highlight)"},
	{Name: "dropMove", Params: []Param{{"x", "float"}, {"y", "float"}}, Doc: "where a drag it takes is over it, in points from its top left"},
	{Name: "drop", Params: []Param{{"drop", "Drop"}}, Doc: "what's dropped on it: the items it takes"},
	{Name: "select", Params: []Param{{"selected", "bool"}}, Doc: "the user selected its List's row, or deselected it"},
	{Name: "selectBegin", Doc: "the user started selecting at its List's row while the list isn't selecting (a two-finger pan on iOS, a long press on Android)"},
	{Name: "focusChange", Params: []Param{{"focused", "bool"}}, Doc: "it took the keyboard's focus, or lost it (a focusable view's)"},
	{Name: "keyText", Params: []Param{{"text", "string"}}, Doc: "text typed while it has the keyboard's focus that its keys (and its containers') didn't take: a listbox's typeahead"},
	{Name: "pressStart", Params: []Param{{"x", "float"}, {"y", "float"}}, Doc: "a press began on it: where, in points from its top left (before pressedChange(true)): a slider's track jumps there"},
}

// Elements are the built-in elements. core/src/ui implements each.
var Elements = []Element{
	{Name: "View", Children: true, Variants: true, Keys: containerKeys, Events: containerEvents, Doc: "a container: a column by default. Without a look (background, border...), events or an accessibility prop it makes no native view: its children go in the nearest one around it"},
	{Name: "Row", Children: true, Variants: true, Keys: containerKeys, Events: containerEvents, Doc: "a container whose children stack on a row"},
	{Name: "Column", Children: true, Variants: true, Keys: containerKeys, Events: containerEvents, Doc: "a container whose children stack on a column"},
	{Name: "SafeArea", Children: true, Variants: true, Keys: containerKeys, Events: containerEvents, Doc: "a column that keeps its children out of the system's bars, notches and the keyboard: padded by the safe area's insets, over its own padding"},
	{Name: "Text", Keys: []Key{
		{Name: "text", Type: "string", Positional: true},
		{Name: "selectable", Type: "bool", Doc: "the text can be selected and copied"},
	}, Doc: "a text: it wraps, unless lines limits it"},
	{Name: "Button", Keys: []Key{
		{Name: "title", Type: "string", Positional: true},
		{Name: "variant", Type: "ButtonVariant", Doc: "how the platform draws it"},
		{Name: "icon", Type: "string", Doc: "a symbol before the title (see Symbol); with no title, the icon alone"},
		{Name: "menu", Type: "[Action]", Doc: "actions shown when it's tapped, instead of press"},
		{Name: "href", Type: "string", Doc: "goes to this route when it's pressed, after press (the Router around it navigates there): an href, or a routed screen's call (href: Thread(id: 7))"},
		{Name: "role", Type: "ActionRole", Doc: "in an Alert or a Screen's bars, what it does, which the platform draws it by: .cancel, .destructive (red)"},
	}, Events: []Event{{Name: "press"}}, Doc: "the platform's button"},
	{Name: "TextField", Keys: []Key{
		{Name: "text", Type: "string", Bind: true, Event: "change", Doc: "its text: typing writes it, and writing it replaces what the field shows"},
		{Name: "placeholder", Type: "string"},
		{Name: "keyboard", Type: "Keyboard"},
		{Name: "returnKey", Type: "ReturnKey"},
		{Name: "secure", Type: "bool", Doc: "a password's: its characters hidden"},
		{Name: "autocapitalize", Type: "Autocapitalize"},
		{Name: "autocorrect", Type: "Autocorrect"},
		{Name: "contentType", Type: "string", Doc: "what it holds, for the system's AutoFill: username, password, newPassword, oneTimeCode, email, ..."},
		{Name: "multiline", Type: "bool", Doc: "lines of text: return is a new line, not submit, and it grows with its text up to lines lines"},
		{Name: "plain", Type: "bool", Doc: "without the platform's border, background and inset: its text alone"},
		{Name: "autofocus", Type: "bool", Doc: "takes the keyboard as it shows"},
		{Name: "selection", Type: "TextRange", Bind: true, Event: "select", Doc: "the cursor or the selected text, as byte offsets into text: the user's moves write it, and writing it moves them"},
		{Name: "rich", Type: "[Span]", Bind: true, Event: "richChange", Doc: "makes it a text area of styled text: bold, italic, underline, strikethrough, monospaced, links. The user's typing and formatting write it; writing it replaces the text, and text follows its plain text. Inline components are left out. It makes the field multiline"},
		{Name: "focused", Type: "bool", Bind: true, Event: "focusChange", Doc: "whether it has the keyboard: the user's tapping in and out writes it, and writing it takes the keyboard or gives it up"},
		{Name: "pasteTypes", Type: "[string]", Doc: "the MIME types (patterns, as \"image/*\") of what on paste takes instead of the text: pasted, or put in by the keyboard (a GIF keyboard's images)"},
	}, Events: []Event{
		{Name: "change", Params: []Param{{"text", "string"}}},
		{Name: "submit", Doc: "return pressed (not in a multiline field)"},
		{Name: "focus"},
		{Name: "blur"},
		{Name: "select", Params: []Param{{"selection", "TextRange"}}},
		{Name: "focusChange", Params: []Param{{"focused", "bool"}}, Doc: "it took the keyboard, or gave it up"},
		{Name: "richChange", Params: []Param{{"rich", "[Span]"}}},
		{Name: "formats", Params: []Param{{"formats", "TextFormats"}}, Doc: "the styles where the cursor is, or that all the selection has, as they change: a toolbar's buttons show them"},
		{Name: "paste", Params: []Param{{"items", "[SharedItem]"}}, Doc: "what was pasted of pasteTypes: images and files are the app's own copies, in its caches"},
	}, Slots: []Slot{
		{Name: "accessory", Doc: "a bar on top of the keyboard while the field is focused (a count and Done, formatting buttons): as wide as the window, as tall as its content, built and gone with the field. Desktops don't show it"},
	}, Doc: "a text field: a line, or lines with multiline"},
	{Name: "Toggle", Keys: []Key{
		{Name: "on", Type: "bool", Bind: true, Event: "change"},
		{Name: "title", Type: "string", Doc: "what screen readers call it (the text beside it is the app's)"},
		{Name: "checkbox", Type: "bool", Doc: "a checkbox rather than a switch"},
	}, Events: []Event{{Name: "change", Params: []Param{{"on", "bool"}}}}, Doc: "a switch, or a checkbox"},
	{Name: "Slider", Keys: []Key{
		{Name: "value", Type: "float", Bind: true, Event: "change"},
		{Name: "min", Type: "float", Default: "0"},
		{Name: "max", Type: "float", Default: "1"},
	}, Events: []Event{{Name: "change", Params: []Param{{"value", "float"}}}}, Doc: "picks a value in a range"},
	{Name: "Stepper", Keys: []Key{
		{Name: "value", Type: "float", Bind: true, Event: "change"},
		{Name: "min", Type: "float", Default: "0"},
		{Name: "max", Type: "float", Default: "100"},
		{Name: "step", Type: "float", Default: "1"},
	}, Events: []Event{{Name: "change", Params: []Param{{"value", "float"}}}}, Doc: "steps a value with minus and plus buttons; show the value beside it"},
	{Name: "Segmented", Keys: []Key{
		{Name: "segments", Type: "[string]", Positional: true},
		{Name: "selected", Type: "int", Bind: true, Event: "change"},
	}, Events: []Event{{Name: "change", Params: []Param{{"index", "int"}}}}, Doc: "one of a few options side by side"},
	{Name: "Picker", Keys: []Key{
		{Name: "items", Type: "[string]", Positional: true},
		{Name: "selected", Type: "int", Default: "-1", Bind: true, Event: "change", Doc: "the chosen item's index; out of range: none, and it shows its prompt"},
		{Name: "prompt", Type: "string", Doc: "what it picks: shown while none is chosen (Android's floating label), and read by screen readers"},
	}, Events: []Event{{Name: "change", Params: []Param{{"index", "int"}}}}, Doc: "picks one of many options from a menu, showing the chosen one"},
	{Name: "Wheel", Keys: []Key{
		{Name: "items", Type: "[string]", Positional: true},
		{Name: "selected", Type: "int", Bind: true, Event: "change", Doc: "the item turned to"},
	}, Events: []Event{{Name: "change", Params: []Param{{"index", "int"}}}}, Doc: "one of many options on a spinning wheel (UIPickerView; Android's NumberPicker)"},
	{Name: "ColorPicker", Keys: []Key{
		{Name: "value", Type: "Color", Bind: true, Event: "change", Doc: "the color chosen"},
		{Name: "alpha", Type: "bool", Doc: "the user can choose an opacity too"},
		{Name: "title", Type: "string", Doc: "the picker's title"},
	}, Events: []Event{{Name: "change", Params: []Param{{"color", "Color"}}}}, Doc: "a swatch of a color that opens the platform's picker (UIColorWell; a Material dialog of a palette on Android)"},
	{Name: "DatePicker", Keys: []Key{
		{Name: "value", Type: "int", Bind: true, Event: "change", Doc: "the instant chosen, in milliseconds since 1970 (std/time's time.Picker binds a Time)"},
		{Name: "mode", Type: "DateMode", Doc: "a date, a time of day, or both"},
		{Name: "min", Type: "int", Doc: "the earliest choice, in milliseconds since 1970 (0: none)"},
		{Name: "max", Type: "int", Doc: "the latest choice (0: none)"},
		{Name: "inline", Type: "bool", Doc: "iOS: the calendar and the time shown in place, not from a button"},
	}, Events: []Event{{Name: "change", Params: []Param{{"value", "int"}}}}, Doc: "a date or a time, chosen the platform's way (UIDatePicker; Material's date and time pickers), in the device's time zone and format"},
	{Name: "Progress", Keys: []Key{
		{Name: "value", Type: "float?", Doc: "done, from 0 to 1; none: under way, how far unknown"},
	}, Doc: "a progress bar; its color is its fill"},
	{Name: "ProgressRing", Keys: []Key{
		{Name: "value", Type: "float?", Doc: "done, from 0 to 1; none: the platform's spinning indicator"},
	}, Doc: "circular progress; its color is its fill"},
	{Name: "Activity", Doc: "a spinner"},
	{Name: "Symbol", Keys: []Key{
		{Name: "name", Type: "string", Positional: true},
	}, Doc: "a symbol (an SF Symbol, the Material Symbol for it), sized as text of its font and colored with its color"},
	{Name: "Image", Keys: []Key{
		{Name: "source", Type: "string", Positional: true, Doc: "an http(s) URL, an asset's path, or a file's"},
		{Name: "mode", Type: "ImageMode", Doc: "how it fills its box"},
		{Name: "placeholder", Type: "string", Doc: "an image shown until source loads, and if it fails"},
		{Name: "transition", Type: "float", Doc: "seconds it takes to fade in as it arrives (0: the platform's; -1: none)"},
		{Name: "blurHash", Type: "string", Doc: "a BlurHash (blurha.sh) shown until source loads, decoded once per hash: at the style's aspectRatio, or square and stretched to the box"},
		{Name: "thumbHash", Type: "string", Doc: "a ThumbHash (base64) shown until source loads: it keeps its image's aspect ratio and alpha"},
		{Name: "placeholderMode", Type: "PlaceholderMode", Doc: "how the placeholder fills the box: as the image does, or its own way"},
		{Name: "animated", Type: "bool?", Doc: "plays an animated image (GIF, APNG, WebP) while true, stops it where it is while false; none: it plays unless the user asked for less motion (Reduce Motion), when it shows its first frame"},
	}, Events: []Event{
		{Name: "loaded", Params: []Param{{"width", "float"}, {"height", "float"}}},
		{Name: "error", Params: []Param{{"message", "string"}}},
	}, Doc: "a picture the host loads; without a size it takes its natural one, no wider than its room, and keeps its aspect ratio when one side is set"},
	{Name: "Scroll", Children: true, Keys: []Key{
		{Name: "horizontal", Type: "bool"},
		{Name: "refreshing", Type: "bool", Doc: "shows the pull to refresh spinner while true (with on refresh)"},
		{Name: "paging", Type: "bool", Doc: "it stops only at multiples of its own size, a page each, and hides its scroll bars"},
		{Name: "page", Type: "int", Bind: true, Event: "pageChange", Doc: "the page it shows (paging): a swipe that settles writes it, and writing it scrolls there"},
	}, Events: []Event{
		{Name: "scroll", Params: []Param{{"x", "float"}, {"y", "float"}}, Doc: "its content offset as it scrolls"},
		{Name: "refresh", Doc: "pulled to refresh"},
		{Name: "pageChange", Params: []Param{{"page", "int"}}, Doc: "a paging swipe is settling on a page: its index from the content's start (its leading edge, right to left)"},
	}, Doc: "scrolls its children along one axis (horizontal: a row); its wrap, justify, align, gap and padding lay out its content. Give it a size (or grow) to scroll within"},
	{Name: "List", Children: true, LazyFor: true, Keys: []Key{
		{Name: "refreshing", Type: "bool", Doc: "shows the pull to refresh spinner while true (with on refresh)"},
		{Name: "columns", Type: "int", Doc: "a grid of this many columns (gap apart)"},
		{Name: "appearance", Type: "ListAppearance"},
		{Name: "anchor", Type: "ScrollAnchor", Doc: ".bottom: a chat's, which rests at its end"},
		{Name: "swipeActions", Type: "fn(int) -> [Action]", Doc: "row index's actions, revealed by swiping it from its trailing edge (a destructive one red; the first a full swipe's)"},
		{Name: "leadingSwipeActions", Type: "fn(int) -> [Action]", Doc: "row index's actions revealed from its leading edge (Mail's Unread)"},
		{Name: "section", Type: "fn(int) -> string", Doc: "row index's section: rows next to each other with the same key are one, headed by header"},
		{Name: "header", Type: "fn(string) -> View", Builder: true, Doc: "a section's header, given its key (an accessor: a recycled header follows its new section)"},
		{Name: "selecting", Type: "bool", Doc: "selection mode (Mail's Edit): rows show the platform's selection control, a tap selects instead of pressing, and rows don't swipe"},
		{Name: "selected", Type: "fn(int) -> bool", Doc: "whether row index is selected, followed as what it reads changes"},
	}, Events: []Event{
		{Name: "scroll", Params: []Param{{"x", "float"}, {"y", "float"}}},
		{Name: "refresh", Doc: "pulled to refresh"},
		{Name: "endReached", Doc: "the last rows are about to show: load more (once per length)"},
		{Name: "startReached", Doc: "the first rows are about to show: load earlier ones (once per first item)"},
		{Name: "select", Params: []Param{{"index", "int"}, {"selected", "bool"}}, Doc: "the user selected or deselected row index; what selected doesn't take back goes back as it was"},
		{Name: "selectBegin", Params: []Param{{"index", "int"}}, Doc: "the user started selecting at row index while the list isn't selecting (a two-finger pan on iOS, a long press on Android)"},
		{Name: "action", Params: []Param{{"index", "int"}, {"action", "int"}}, Doc: "a swipe action of row index was chosen: its index among swipeActions, or -1 - its index among leadingSwipeActions (its run has run)"},
	}, Doc: "a for in it is the platform's recycling list: rows exist only while they're on screen. It grows into its room unless it's sized"},
	{Name: "RichText", Children: false, Keys: []Key{
		{Name: "text", Type: "[Span]", Positional: true},
	}, Events: []Event{
		{Name: "link", Params: []Param{{"url", "string"}}, Doc: "a span with a link tapped"},
	}, Doc: "styled paragraphs (spans), typeset and drawn by the platform's text engine; the style's font, color, textAlign and lines are its spans' defaults. Each paragraph's layout is cached by its content and width"},
	{Name: "Selectable", Children: true, Doc: "makes the RichTexts inside it selectable as one text, across them: a long press selects a word, the handles extend it over any of them, the edit menu copies it"},
	{Name: "Canvas", Keys: []Key{
		{Name: "draw", Type: "fn(Graphics)", Positional: true, Doc: "draws with g, in points from the canvas's top left: called once it's laid out, again when its size changes and when what it read changes; only a drawing that differs is sent"},
	}, Doc: "draws what draw draws: a chart, a gauge, a signature. It takes the size its style gives it; with none, it's empty"},
	{Name: "Shape", Keys: []Key{
		{Name: "path", Type: "fn(float, float) -> Path", Positional: true, Doc: "the outline for its size (width, height), in points"},
		{Name: "fill", Type: "Color"},
		{Name: "stroke", Type: "Color"},
		{Name: "lineWidth", Type: "float", Default: "1"},
		{Name: "lineCap", Type: "LineCap"},
		{Name: "lineJoin", Type: "LineJoin"},
		{Name: "trimStart", Type: "float", Default: "0", Doc: "the part of the outline drawn, from trimStart to trimEnd (0 to 1): a progress ring; it animates in animate { }"},
		{Name: "trimEnd", Type: "float", Default: "1"},
		{Name: "dash", Type: "[float]", Doc: "lengths drawn and skipped in turn; []: solid"},
		{Name: "dashPhase", Type: "float"},
	}, Doc: "an outline, filled, stroked or both. It takes the size its style gives it"},
	{Name: "Shader", Children: true, Keys: []Key{
		{Name: "program", Type: "bytes", Positional: true, Doc: "the compiled shader: shader(\"\"\"…\"\"\"), a Kage program the compiler compiles"},
		{Name: "uniforms", Type: "[Uniform]", Doc: "its uniform variables' values, by name; a change draws it again"},
		{Name: "animated", Type: "bool", Doc: "runs its Time: drawn again every frame the display draws"},
	}, Doc: "a box a GPU program draws, under its children, over its background"},
	{Name: "Video", Keys: []Key{
		{Name: "source", Type: "string", Positional: true, Doc: "an http(s) URL (a file, an HLS stream), or an asset"},
		{Name: "mode", Type: "ImageMode", Doc: ".fit (letterboxed) or .fill (cropped)"},
		{Name: "hideControls", Type: "bool"},
		{Name: "autoPlay", Type: "bool"},
		{Name: "loop", Type: "bool"},
		{Name: "muted", Type: "bool"},
	}, Events: []Event{
		{Name: "end", Doc: "played to its end (each time, looping)"},
		{Name: "error", Params: []Param{{"message", "string"}}},
	}, Doc: "a video in the platform's player; sized as an Image is. Drive it with play(), pause(), seek(seconds)"},
	{Name: "WebView", Keys: []Key{
		{Name: "url", Type: "string", Positional: true, Default: "\"\"", Doc: "the page: an http(s) URL, or an asset (\"\": html's)"},
		{Name: "html", Type: "string", Doc: "markup to show instead"},
		{Name: "preload", Type: "string", Doc: "a script run before each page's own, in every frame (set when it's made)"},
		{Name: "userAgent", Type: "string"},
		{Name: "block", Type: "[string]", Doc: "requests kept from loading: a host (and its subdomains), or with a /, a part of the URL (set when it's made)"},
	}, Events: []Event{
		{Name: "loaded", Params: []Param{{"url", "string"}}, Doc: "a page loaded"},
		{Name: "error", Params: []Param{{"message", "string"}}},
		{Name: "message", Params: []Param{{"message", "string"}}, Doc: "what the page posts: window.asuka.postMessage(text)"},
		{Name: "state", Params: []Param{{"state", "WebState"}}},
		{Name: "request", Params: []Param{{"url", "string"}}, Doc: "each request the page makes (Android)"},
		{Name: "download", Params: []Param{{"download", "WebDownload"}}},
		{Name: "newWindow", Params: []Param{{"url", "string"}}, Doc: "a page it would open in a new window; without a handler it loads here"},
	}, Doc: "a web page, in the platform's web view; with no height and no grow, it grows"},
	// Navigation: stacks, tabs, routers, split views, windows.
	{Name: "Stack", Children: true, LazyChildren: true, Doc: "a navigation stack (UINavigationController; a view stack with a top app bar and predictive back on Android): its child is its first screen; push(…) and pop() in it add and remove more, and the user's back pops. It grows into its room unless it's sized"},
	{Name: "Screen", Children: true, Keys: []Key{
		{Name: "title", Type: "string", Doc: "the bar's title (a tab's, a page's in a browser)"},
		{Name: "largeTitle", Type: "bool", Doc: "the title large at the top, collapsing into the bar as it scrolls (iOS)"},
		{Name: "backGesture", Type: "bool", Default: "true", Doc: "the edge swipe pops it (iOS)"},
		{Name: "zoomFrom", Type: "string", Doc: "the shared key of a view on the screen below: the screen zooms out of it as it's pushed, and back as it's popped (iOS 18)"},
		{Name: "barColor", Type: "Color", Doc: "the top app bar's color while it's on top (Android)"},
		{Name: "search", Type: "string", Bind: true, Event: "searchChange", Doc: "a search field in the bar: what's typed (typing writes it)"},
		{Name: "searchPlaceholder", Type: "string", Doc: "the search field's placeholder (\"\": Search, in the app's language)"},
		{Name: "searchButton", Type: "bool", Doc: "the search field stays out of the way until it's asked for"},
		{Name: "primaryExtended", Type: "bool", Doc: "primary's title beside its icon, shrinking to the icon as the content scrolls (Android's extended FAB)"},
	}, Events: []Event{
		{Name: "searchChange", Params: []Param{{"text", "string"}}, Doc: "the search field's text changed"},
		{Name: "searchSubmit", Doc: "return pressed in the search field"},
	}, Slots: []Slot{
		{Name: "toolbar", Doc: "the bar's buttons: Buttons (title or icon, menu, disabled), shown by the platform's bar (iOS's navigation bar, Android's top app bar)"},
		{Name: "bottomToolbar", Doc: "buttons in a bar at the bottom of the screen: Buttons, as toolbar's"},
		{Name: "primary", Doc: "the screen's main action (compose, add): a Button; Android's floating action button, iOS's prominent toolbar item"},
	}, Doc: "a screen of a Stack (a screen declaration's root), sized by the platform"},
	{Name: "Tabs", Children: true, Keys: []Key{
		{Name: "selected", Type: "int", Bind: true, Event: "change", Doc: "the shown tab's index: the user's choice writes it"},
	}, Events: []Event{{Name: "change", Params: []Param{{"index", "int"}}}}, Doc: "pages with a tab bar (UITabBarController; Material's navigation bar): its children are Tabs. It grows into its room unless it's sized"},
	{Name: "Tab", Children: true, LazyChildren: true, Keys: []Key{
		{Name: "title", Type: "string", Positional: true},
		{Name: "icon", Type: "string", Doc: "a symbol"},
		{Name: "badge", Type: "string", Doc: "shown on the tab: a count (\"3\"), or short text; empty: none"},
	}, Doc: "a page of Tabs: its content is built when it's first chosen"},
	{Name: "Sheet", Children: true, LazyChildren: true, Keys: []Key{
		{Name: "open", Type: "bool", Bind: true, Event: "change", Doc: "shows the sheet while true; the user dismissing it writes false"},
		{Name: "detents", Type: "[Detent]", Doc: "the heights it rests at ([]: large)"},
		{Name: "grabber", Type: "bool"},
		{Name: "fullScreen", Type: "bool", Doc: "covers the screen; the user can't swipe it away"},
		{Name: "movable", Type: "bool", Doc: "on a desktop, a dialog the user can move (a Mac's modal window, the web's panel) rather than a sheet attached to the window; phones ignore it"},
		{Name: "resizable", Type: "bool", Doc: "with movable, the user can also resize it; a movable sheet isn't resizable without it, as a desktop's dialogs aren't"},
	}, Events: []Event{
		{Name: "change", Params: []Param{{"open", "bool"}}},
		{Name: "dismiss", Doc: "the user dismissed it"},
	}, Doc: "content in a modal sheet while open: built as it opens, disposed as it closes. It takes no room where it's declared"},
	{Name: "Alert", Children: true, Keys: []Key{
		{Name: "title", Type: "string", Positional: true},
		{Name: "open", Type: "bool", Bind: true, Event: "change", Doc: "shows the alert while true; choosing a button, or the user dismissing it, writes false"},
		{Name: "message", Type: "string", Doc: "under the title"},
		{Name: "actionSheet", Type: "bool", Doc: "an action sheet: a list of choices from the bottom of the screen (a popover on an iPad), its fields left out"},
	}, Events: []Event{
		{Name: "change", Params: []Param{{"open", "bool"}}},
		{Name: "dismiss", Doc: "the user dismissed it without choosing a button (a tap outside, Android's back)"},
	}, Doc: "an alert while open: its block's Buttons are its buttons (their title, icon, role and disabled; a cancel one's press runs as it's dismissed too), its TextFields its fields (placeholder, bind text, secure, keyboard, contentType). It takes no room where it's declared; with no Button, it has an OK"},
	{Name: "Popover", Children: true, Keys: []Key{
		{Name: "open", Type: "bool", Bind: true, Event: "change", Doc: "shows the popover while true; a tap outside writes false"},
		{Name: "maxWidth", Type: "float", Doc: "how wide it may be (0: 320); it's as big as its content"},
	}, Events: []Event{
		{Name: "change", Params: []Param{{"open", "bool"}}},
		{Name: "dismiss", Doc: "the user dismissed it"},
	}, Slots: []Slot{
		{Name: "content", Lazy: true, Doc: "what the popover shows: built as it opens, disposed as it closes"},
	}, Doc: "content in a bubble pointing at its children while open (UIPopoverPresentationController, a PopupWindow, NSPopover)"},
	{Name: "Overlay", Children: true, LazyChildren: true, Keys: []Key{
		{Name: "open", Type: "bool", Bind: true, Event: "change", Doc: "shows its children while true: built as it opens, disposed as it closes (a change in animate { } plays their transition)"},
		{Name: "anchor", Type: "Ref?", Doc: "the view it's placed next to: none, in its window (a dialog, a drawer, a toast)"},
		{Name: "side", Type: "Side", Doc: "the anchor's side it goes on (.bottom, .top, .start, .end), or .center: over it. Without an anchor, the window's"},
		{Name: "align", Type: "Align", Doc: "where on that side: .start, .center (or .auto), .end; .stretch: as long as the anchor on that side"},
		{Name: "offset", Type: "float", Doc: "the gap between it and the anchor"},
		{Name: "alignOffset", Type: "float", Doc: "moves it along the side, from where align puts it"},
		{Name: "flip", Type: "bool", Default: "true", Doc: "goes to the opposite side when it doesn't fit on its own and fits better there"},
		{Name: "edgeGap", Type: "float", Default: "8", Doc: "the least distance it keeps from the window's edges (inside the safe area): it's moved along its side, and made no bigger than the room left"},
		{Name: "matchWidth", Type: "bool", Doc: "at least as wide as the anchor (a select's list)"},
		{Name: "modal", Type: "bool", Doc: "presses reach only it while it's open (a backdrop's elsewhere), and screen readers see only it"},
		{Name: "dismissOn", Type: "[DismissReason]", Default: "[.outsidePress, .escape, .back, .focusOut, .anchorHidden]", Doc: "what closes it: a press outside it (not on the anchor), Escape, Back (Android's, the web's Escape), the keyboard's focus leaving it and its anchor, the anchor scrolled or hidden out of view. Each writes open false and reports dismiss"},
	}, Events: []Event{
		{Name: "change", Params: []Param{{"open", "bool"}}},
		{Name: "dismiss", Params: []Param{{"reason", "DismissReason"}}, Doc: "the user dismissed it"},
		{Name: "place", Params: []Param{{"placed", "Placed"}}, Doc: "where it went, once it's placed and each time that changes: the side after flipping (an arrow's side, a transition's direction), where the anchor's middle is along that side, the room it has"},
	}, Slots: []Slot{
		{Name: "backdrop", Lazy: true, Doc: "what covers its window behind it while it's open (a dialog's dimming): as big as the window, built and disposed with the children"},
	}, Doc: "its children over the window, placed next to an anchor while open: a select's list, a menu, a tooltip, a dialog. Not the platform's popover: what shows is only what it holds, styled by the app (std/base's popups are made of it)"},
	{Name: "Menu", Children: true, Keys: []Key{
		{Name: "title", Type: "string", Positional: true, Default: "\"\"", Doc: "in a MenuBar, the menu's title (File, Edit)"},
		{Name: "actions", Type: "[Action]", Doc: "the menu's actions"},
		{Name: "longPress", Type: "bool", Doc: "a context menu: shown on a long press (a right click on a desktop) rather than a tap"},
	}, Doc: "shows a menu of actions when its children are tapped (UIMenu, PopupMenu). In a MenuBar, one of its menus: its children are MenuItems"},
	{Name: "Split", Children: true, Keys: []Key{
		{Name: "title", Type: "string", Positional: true, Default: "\"\"", Doc: "the sidebar's"},
		{Name: "selected", Type: "int", Bind: true, Event: "change", Doc: "the chosen destination's index: the user's choice writes it"},
	}, Events: []Event{{Name: "change", Params: []Param{{"index", "int"}}}}, Doc: "destinations in a sidebar and the chosen one's screens beside it, as a Stack (UISplitViewController; Material's navigation drawer): its children are Destinations, or Sidebars of them"},
	{Name: "Destination", Children: true, LazyChildren: true, Keys: []Key{
		{Name: "title", Type: "string", Positional: true},
		{Name: "icon", Type: "string"},
		{Name: "badge", Type: "string"},
		{Name: "section", Type: "string", Doc: "the sidebar section it's in: destinations next to each other with the same section are one"},
	}, Doc: "a place a Split's sidebar leads to: its content is its first screen, built when it's chosen"},
	{Name: "Sidebar", Children: true, Keys: []Key{
		{Name: "title", Type: "string", Positional: true, Doc: "the section's heading in the sidebar"},
	}, Doc: "a section of a Split's sidebar: its children are Destinations, under its title (a Destination's own section, if it gives one, wins)"},
	{Name: "Window", Children: true, LazyChildren: true, NoStyle: true, Keys: []Key{
		{Name: "open", Type: "bool", Bind: true, Event: "change", Doc: "the window is open while true; the user closing it writes false. The app block's Window, the main window, shows unless it's false"},
		{Name: "title", Type: "string", Doc: "its title bar's (\"\": the app's name)"},
		{Name: "width", Type: "float", Doc: "its content's size as it opens, in points (0: the platform's, 800 by 600)"},
		{Name: "height", Type: "float"},
		{Name: "minWidth", Type: "float", Doc: "the least the user can make it (0: the platform's)"},
		{Name: "minHeight", Type: "float"},
		{Name: "maxWidth", Type: "float", Doc: "the most the user can make it (0: no most)"},
		{Name: "maxHeight", Type: "float"},
		{Name: "x", Type: "float", Doc: "its frame's top left on the screen, in points from the main display's (both 0: cascaded from the window in use, or centered with center)"},
		{Name: "y", Type: "float"},
		{Name: "center", Type: "bool", Doc: "opens centered on its display"},
		{Name: "resizable", Type: "bool", Default: "true", Doc: "the user can resize it"},
		{Name: "minimizable", Type: "bool", Default: "true"},
		{Name: "maximizable", Type: "bool", Default: "true"},
		{Name: "closable", Type: "bool", Default: "true", Doc: "it has a close button"},
		{Name: "alwaysOnTop", Type: "bool", Doc: "above other apps' windows (not under Wayland)"},
		{Name: "hidden", Type: "bool", Doc: "opens without showing: its content is built and laid out, and w.show() shows it ready"},
		{Name: "skipTaskbar", Type: "bool", Doc: "left out of the taskbar (Windows) and the Window menu (macOS)"},
		{Name: "transparent", Type: "bool", Doc: "what's behind the window shows where the content draws nothing (macOS, Linux with a compositor)"},
		{Name: "maximized", Type: "bool", Doc: "opens maximized"},
		{Name: "fullScreen", Type: "bool", Doc: "opens in full screen"},
		{Name: "titleBar", Type: "TitleBar", Doc: ".hidden: the content goes under the title bar, the window's buttons over it (environment().safeTop is the bar's height)"},
		{Name: "background", Type: "Color", Doc: "fills the window behind its content (none: the platform's)"},
		{Name: "child", Type: "bool", Doc: "belongs to the window it's declared in: stays above it, closes with it"},
		{Name: "modal", Type: "bool", Doc: "a child that blocks the window it's declared in while it's open (on a Mac, a sheet of it)"},
		{Name: "panel", Type: "bool", Doc: "a launcher's or a palette's window: frameless, above every app's windows, out of the taskbar, taking the keyboard without bringing the app to the front; Escape and a click elsewhere close it (on closeRequest: hide it instead)"},
		{Name: "flyout", Type: "Flyout?", Doc: "a frameless window hanging from an anchor (a tray icon: platform/tray's anchor(id); or a rectangle of the window it's declared in), closed as it loses the keyboard"},
		{Name: "fitContent", Type: "bool", Doc: "sized to its content, within its least and most size, following it as it changes"},
		{Name: "clickThrough", Type: "bool", Doc: "the mouse goes through it to what's behind (an overlay's, a HUD's)"},
		{Name: "menuBar", Type: "bool", Doc: "shows the app's MenuBar as the main window does (Windows' menu bar, Linux's main menu; a Mac's is the app's)"},
		{Name: "stateKey", Type: "string", Doc: "where the platform remembers where the user left it, across launches (the main window's: \"main\" when it's empty)"},
		{Name: "edited", Type: "bool", Doc: "its document has unsaved changes"},
		{Name: "file", Type: "string", Doc: "the path of the file it shows"},
	}, Events: []Event{
		{Name: "change", Params: []Param{{"open", "bool"}}},
		{Name: "close", Doc: "the user closed it"},
		{Name: "closeRequest", Doc: "the user asked to close it: it stays open until open is false, or window.close()"},
	}, Doc: "a desktop window of its own while open (a window in the page on the web; phones have none): its content is built as it opens, disposed as it closes. The app block's Window is the main window: its keys are the main window's"},
	{Name: "Router", Children: true, Keys: []Key{
		{Name: "initial", Type: "string", Positional: true, Default: "\"/\"", Doc: "where it starts when nothing else says (a link the app was opened with, the address its window was left at)"},
	}, Doc: "shows its routes' screens from the address the app is at: its children are Routes. Router(initial: \"/inbox\") { … }"},
	{Name: "Route", Children: true, LazyChildren: true, Params: []Param{{"params", "Params"}}, Keys: []Key{
		{Name: "path", Type: "string", Positional: true, Default: "\"\"", Doc: "relative to its parent's unless it starts with /: :name a parameter, *name the rest; \"\" its parent's index"},
		{Name: "layout", Type: "RouteLayout", Positional: true, Default: ".group", Doc: "a navigator of its child routes (.stack, .tabs, .split, .slot), or a group of them under its path"},
		{Name: "title", Type: "string", Positional: true, Default: "\"\"", Doc: "its tab's or destination's title, and its screens' when they have none"},
		{Name: "icon", Type: "string", Positional: true, Default: "\"\"", Doc: "its tab's or destination's symbol"},
		{Name: "badge", Type: "string", Positional: true, Default: "\"\"", Doc: "shown on its tab: a count, or short text"},
		{Name: "initial", Type: "string", Positional: true, Default: "\"\"", Doc: "the child a navigator starts with (a stack's first screen, under the one a link names)"},
		{Name: "redirect", Type: "string", Positional: true, Default: "\"\"", Doc: "sends whoever comes here to this href, its parameters filled in"},
		{Name: "sheet", Type: "bool", Positional: true, Default: "false", Doc: "presented in a sheet over its navigator"},
		{Name: "detents", Type: "[Detent]", Positional: true, Default: "[]", Doc: "a sheet's heights ([]: large)"},
		{Name: "grabber", Type: "bool", Positional: true, Default: "false", Doc: "a sheet's grabber, at its top"},
		{Name: "fullScreen", Type: "bool", Positional: true, Default: "false", Doc: "a sheet that covers the screen: no swipe or tap outside dismisses it"},
		{Name: "guard", Type: "(fn(string) -> string)?", Positional: true, Default: "none", Doc: "asked before its screens show, with the href asked for: \"\" lets the user in, an href sends them there"},
	}, Doc: "maps a path to a screen, or holds routes: Route(\"/inbox\", layout: .stack, title: \"Inbox\") { Inbox; Thread; Route(\":id/reply\", sheet: true) { p => Reply(id: p.get(\"id\")) } }. Its block holds its child Routes, its screen (p => View, built with its Params), or routed screens named alone (screen Thread(id: int) at \":id\"), a Route of their pattern each"},
	{Name: "Boundary", Children: true, LazyChildren: true, Events: []Event{
		{Name: "error", Params: []Param{{"error", "Error"}, {"reset", "fn()"}}, Builds: true, Doc: "what shows instead when building its content, or an effect or handler in it, fails; reset builds it again"},
	}, Doc: "an error boundary: an error its content doesn't catch shows on error's views"},
	{Name: "PageControl", Keys: []Key{
		{Name: "count", Type: "int", Positional: true},
		{Name: "page", Type: "int", Bind: true, Event: "change"},
	}, Events: []Event{{Name: "change", Params: []Param{{"page", "int"}}}}, Doc: "a row of dots, one per page, the current one highlighted (UIPageControl)"},
	// Desktop: windows and their handles.
	{Name: "MenuBar", Children: true, LazyChildren: true, NoStyle: true, Doc: "the app's menus (macOS's menu bar; Windows' and Linux's in the main window), with their keyboard shortcuts, beside the platform's own: its children are Menus. While several show, the last one's are the app's; when it goes, the one before's. Phones ignore it"},
	{Name: "MenuItem", Children: true, NoStyle: true, Keys: []Key{
		{Name: "title", Type: "string", Positional: true, Doc: "what it says (\"\": its role's own)"},
		{Name: "shortcut", Type: "Shortcut?", Doc: "chooses it from the keyboard, in every window of the app"},
		{Name: "role", Type: "MenuRole", Doc: ".destructive draws it red; .quit, .close and .openRecent are the platform's own items"},
		{Name: "disabled", Type: "bool"},
		{Name: "checked", Type: "bool", Doc: "a check mark beside it"},
	}, Events: []Event{
		{Name: "press", Doc: "the user chose it (a click, its shortcut)"},
	}, Doc: "an item of a MenuBar's Menu: MenuItems in it make it a submenu"},
	{Name: "MenuSeparator", NoStyle: true, Doc: "a line between a menu's groups of items"},
	{Name: "WindowCloseGuard", NoStyle: true, Keys: []Key{
		{Name: "when", Type: "bool", Default: "true", Doc: "it asks while true"},
	}, Events: []Event{
		{Name: "closeRequest", Doc: "the user asked to close the window it's in: the window stays open (window.close() closes it)"},
	}, Doc: "keeps the window it's in open while when is true, asking the app first (unsaved changes) when the user closes it"},
	{Name: "WindowKeys", NoStyle: true, Keys: []Key{
		{Name: "keys", Type: "[Shortcut]", Doc: "what they run"},
	}, Doc: "keys of the window it's in while it shows, whatever has the keyboard's focus: the ones no view takes (the last WindowKeys' win)"},
}
