function setStyle(element, ...styles) {
	if (element && element.constructor === String) {
		// allow to reference the splitter using id.
		element = document.getElementById(element);
	}

	let cycleIndex = 0;
	let cycleItems = [];
	let changedItems = [];
	for (let style of styles) {

		// remove style
		if (style.startsWith('-')) {
			element.classList.remove(style.substring(1));
			changedItems.push(style.substring(1));
			continue;
		}

		// toggle style
		if (style.startsWith('!')) {
			style = style.substring(1);
			if (element.classList.contains(style)) {
				element.classList.remove(style);
				changedItems.push(style);
				continue;
			}
		}

		// cycle style
		if (style.startsWith('^')) {
			style = style.substring(1);
			cycleItems.push(style);
			if (element.classList.contains(style)) {
				element.classList.remove(style);
				changedItems.push(style);
				cycleIndex = cycleItems.length;
			}
			continue;
		}

		if (element.classList.contains(style)) {
			// style alredy set
			continue;
		}
		element.classList.add(style);
		changedItems.push(style);
	}
	if (cycleItems.length > 0) {
		let style = cycleItems[cycleIndex % cycleItems.length];
		if (style !== '') {
			element.classList.add(style);
			changedItems.push(style);
		}
	}
	if (element === document.body) {
		if (changedItems.includes('editor') || changedItems.includes('output')) {
			editor.refresh();
		}
	}
}

var props = props || {};
props.title = document.title;
const pathMather = /(?=[\w\/])((?:\w+:\/\/)(?:[^:@\n\r"'`]+(?:\:\w+)?@)?(?:[^:/?#\n\r"'`]+)(?:\:\d+)?(?=[/?#]))?([^<>=:;,?#*|\n\r"'`]*)?(?:\:(?:(\d+)(?:\:(\d+))?))?(?:\?([^#\n\r"'`]*))?(?:\#([^\n\r"'`]*))?/;
const pathFinder = new RegExp(pathMather, 'g');

CodeMirror.commands.save = function(cm) {
	saveInput();
}
CodeMirror.commands.jumpToLine = function(cm) {
	completeAction(':');
}
CodeMirror.commands.scrollToLine = function(cm) {
	let line = editor.getCursor().line || 1;
	var middleHeight = editor.getScrollerElement().offsetHeight / 2;
	var t = editor.charCoords({line, ch: 0}, "local").top;
	editor.scrollTo(null, t - middleHeight - 5);
	editor.setCursor(line, column);
}

CodeMirror.commands.find = function(cm) {
	completeAction('?');
}

CodeMirror.commands.findAndSelect = function(cm) {
	completeAction('?!');
}

CodeMirror.commands.findNext = function(cm) {
	let cursor = edtFileName.cursor;
	if (cursor && cursor.findNext()) {
		editor.setSelection(cursor.from(), cursor.to());
	} else {
		actionError();
	}
}
CodeMirror.commands.findPrev = function(cm) {
	let cursor = edtFileName.cursor;
	if (cursor && cursor.findPrevious()) {
		editor.setSelection(cursor.from(), cursor.to());
	} else {
		actionError();
	}
}

let editor = CodeMirror.fromTextArea(input, {
	mode: "text/x-cmpl",
	lineNumbers: true,
	tabSize: 4,
	indentUnit: 4,
	indentWithTabs: true,
	keyMap: "extraKeys",

	styleActiveLine: true,
	matchBrackets: true,
	showTrailingSpace: true,

	foldGutter: true,
	gutters: ["CodeMirror-linenumbers", "breakpoints", "CodeMirror-foldgutter"],
	highlightSelectionMatches: {showToken: /\w/, annotateScrollbar: true}
});
let terminal = Terminal(output, function(escaped, text) {
	return escaped.replace(pathFinder, function (match, host, path, line, column, query, hash) {
		if (path === undefined) {
			return match;
		}

		line = +(line || 0);
		column = +(column || 0);
		if (host !== undefined) {
			return '<a href="' + encodeURI(match) + '" target="_blank">' + match + '</a>';
		}
		if (line > 0) {
			return '<a href="javascript:void(params.update({file:\'' + path + '\', line:' + line + ', content: null}));">' + match + '</a>';
		}
		return match;
	});
});
let params = JsArgs('#', function (params, changes) {
	//console.trace('params: ', changes, params);

	// set window title
	let title = params.file || '';
	if (params.line) {
		title += ':' + params.line;
	}
	if (params.folder) {
		title += '][' + params.folder;
	}
	document.title = props.title + ' [' + title + ']';

	// no changes => page loaded
	if (changes === undefined) {
		// setup theme, only after loading
		if (params.theme != null) {
			setStyle(document.body, '-dark', '-light', params.theme || 'dark');
		}

		// custom layout, only after loading
		if (params.show != null) {
			setStyle(document.body, '-left-bar', '-editor', '-output', params.show);
		}

		// setup editor content, only after loading
		if (params.content != null) {
			let content = params.content;
			try {
				content = atob(content);
			} catch (e) {
				console.debug(e);
			}
			setContent(content, params.file, params.line);
		}
		return;
	}

	// reload page if the project was modified
	let project = [];
	if (changes.project !== undefined) {
		if (changes.project != params.project) {
			// page needs to be reloaded
			params.update(true);
			return;
		}

		let content = params.project;

		try {
			content = atob(content);
		} catch (e) {
			console.debug(e);
		}

		if (content != null) {
			if (content.startsWith('[{')) {
				project = JSON.parse(content);
			} else {
				for (let url of content.split(';')) {
					project.push({ url });
				}
			}
		}
		if (changes.workspace === undefined) {
			changes.workspace = true;
		}
	}

	if (changes.workspace !== undefined) {
		openProjectFile({
			workspace: params.workspace,
			folder: params.folder,
			file: params.file,
			line: params.line,
			project
		});
		if (params.workspace && params.project) {
			params.project = null;
			return true;
		}
		return;
	}

	if (changes.folder !== undefined) {
		listFiles(params.folder || '.');
	}

	if (changes.file !== undefined) {
		if (params.file == null && params.content == null) {
			// close the current file
			setContent('', '');
			return;
		}
		openProjectFile({
			file: params.file,
			line: params.line
		});
		return;
	}

	if (changes.line !== undefined) {
		setContent(undefined, params.file, params.line);
	}

	if (changes.content !== undefined) {
		let content = params.content;
		if (content != null) {
			try {
				content = atob(content);
			} catch (e) {
				console.debug(e);
			}
		}
		setContent(content, params.file, params.line);
	}
});

function actionError() {
	edtFileName.classList.add('error');
	setTimeout(function() {
		edtFileName.classList.remove('error');
	}, 250);
	return false;
}

editor.on("gutterClick", function(cm, n) {
	let info = cm.lineInfo(n);
	if (info.gutterMarkers == null) {
		let marker = document.createElement("div");
		marker.classList.add("breakpoint");
		marker.innerHTML = "●";
		marker.value = n + 1;
		cm.setGutterMarker(n, "breakpoints", marker);
	} else {
		cm.setGutterMarker(n, "breakpoints", null);
	}
});
editor.setSize('100%', '100%');
editor.focus();

edtFileName.onfocus = function() {
	edtFileName.selectionStart = 0;
	edtFileName.selectionEnd = edtFileName.value.length;
}
edtFileName.onblur = function() {
	edtFileName.value = params.file || '';
	editor.focus();
}
edtFileName.onkeydown = function(event) {
	if (event.key !== 'Enter') {
		return true;
	}

	// `#arguments` => execute script
	if (edtFileName.value.startsWith('#')) {
		let command = edtFileName.value.substr(1);
		//params.update({exec: command});
		execute(command, true);
	}

	// `:%` => zoom
	else if (edtFileName.value.startsWith(':%')) {
		document.body.style.fontSize = (+edtFileName.value.substr(2) / 100) + 'em';
		editor.refresh();
	}

	// `:<number>` => goto line
	else if (edtFileName.value.startsWith(':')) {
		let line = edtFileName.value.substr(1);
		if (!isNaN(line)) {
			params.update({line: line});
		} else {
			editor.execCommand(line);
		}
	}

	// ?! => find and select occurrences
	else if (edtFileName.value.startsWith('?!')) {
		let text = edtFileName.value.substr(2);
		let cursor = editor.getSearchCursor(text, 0);
		let selections = [];
		while (cursor.findNext()) {
			selections.push({
				anchor: cursor.from(),
				head: cursor.to()
			});
		}
		if (selections.length < 1) {
			edtFileName.cursor = null;
			return actionError();
		}
		editor.setCursor(selections[0].head);
		editor.setSelections(selections);
	}

	// ? => find text
	else if (edtFileName.value.startsWith('?')) {
		let text = edtFileName.value.substr(1);
		let cursor = editor.getSearchCursor(text, 0);
		if (!cursor.findNext()) {
			edtFileName.cursor = null;
			return actionError();
		}
		editor.setSelection(cursor.from(), cursor.to());
		edtFileName.cursor = cursor;
	}

	// '[{...' => project
	else if (edtFileName.value.startsWith('[{')) {
		openProjectFile({
			project: JSON.parse(edtFileName.value),
			file: params.file,
			line: params.line
		});
	}

	// `+` => unfoldAll
	else if (edtFileName.value === '+') {
		editor.execCommand('unfoldAll');
	}

	// `-` => foldAll
	else if (edtFileName.value === '-') {
		editor.execCommand('foldAll');
	}

	// close file
	else if (edtFileName.value === '') {
		params.update({ content: null, file: null });
	}

	// open or download file
	else {
		// [match, host, path, line, column, query, hash]
		let match = edtFileName.value.match(pathMather);
		if (match == null) {
			return actionError();
		}

		let file = match[2];
		let line = match[3];
		if (match[1] != null) {
			file = file.replace(/^(.*[/])?(.*)(\..*)$/, "$2$3");
			openProjectFile({ file, project: [{
				file, url: match[1] + match[2]
			}]});
		}
		else if (file.endsWith('/') || file.endsWith('*')) {
			params.update({ folder: match.input });
		}
		else {
			params.update({ content: null, file, line });
		}
	}

	edtFileName.blur();
	return false;
}
edtFileName.onkeyup = completeAction;

window.onkeydown = function() {
	// escape to editor
	if (event.key == 'Escape') {
		if (editor.hasFocus()) {
			setStyle(document.body, '!output', 'editor');
		} else {
			setStyle(document.body, 'editor');
		}
		if (!document.body.classList.contains('canvas')) {
			editor.setSelection(editor.getCursor());
			editor.focus();
		}
		return false;
	}
	// Ctrl + Enter => Execute script
	if (event.ctrlKey && !event.altKey && !event.shiftKey && event.key === 'Enter') {
		execute((params.exec || '-run/g'));
		return false;
	}
	// Shift + Enter => Focus command: [search, jump, fold, run, ...]
	if (!event.ctrlKey && !event.altKey && event.shiftKey && event.key === 'Enter') {
		completeAction();
		return false;
	}
	// Ctrl + Shift + Enter => Execute script
	if (event.ctrlKey && !event.altKey && event.shiftKey && event.key === 'Enter') {
		completeAction('#');
		return false;
	}
}

function completeAction(action, hint) {
	setStyle(document.body, 'tool');
	edtFileName.focus();

	if (action == null) {
		return;
	}
	if (action.constructor === KeyboardEvent) {
		if (!edtFileName.value.endsWith(action.key)) {
			// maybe backspace was pressed
			return;
		}
		action = edtFileName.value;
	}
	if (hint == null) {
		switch (action) {
			default:
				hint = '';
				break;

			case ':':
				hint = editor.getCursor().line + 1 || 'enter a line number to go to';
				break;

			case ':%':
				hint = '100';
				break;

			case '?':
				hint = editor.getSelection() || 'enter a text to search';
				break;

			case '?!':
				hint = editor.getSelection() || 'enter a text to search and select occurrences';
				break;

			case '#':
				hint = terminal.command || '';
				break;
		}
	}

	edtFileName.value = action + hint;
	edtFileName.selectionStart = action.length;
	edtFileName.selectionEnd = edtFileName.value.length;
}

function setContent(content, file, line, column) {
	let contentSet = false;
	if (content != null && content != editor.getValue()) {
		setStyle(document.body, 'editor');
		editor.setValue(content);
		editor.clearHistory();
		contentSet = true;
	}
	if (file != null) {
		edtFileName.value = file;
		for (let li of fileList.children) {
			setStyle(li, li.innerText === file ? "active" : "-active");
		}
	}
	if (line != null) {
		var middleHeight = editor.getScrollerElement().offsetHeight / 2; 
		var t = editor.charCoords({line, ch: 0}, "local").top; 
		editor.scrollTo(null, t - middleHeight - 5);
		editor.setCursor((line || 1) - 1, column);
		setStyle(document.body, 'editor');
	}
	return contentSet;
}

function editProject() {
	if (params.project != null) {
		let files = [];
		let content = params.project;

		try {
			if (content != null) {
				content = atob(content);
			}
		} catch (e) {
			console.warn(e);
		}

		if (content.startsWith('[{')) {
			files = JSON.parse(content);
		} else {
			for (let url of content.split(';')) {
				files.push({ url });
			}
		}
		content = '';
		for (let file of files) {
			if (content !== '') {
				content += ", ";
			}
			content += JSON.stringify(file, null, '\t');
		}
		content = '[' + content + ']';
		params.update({content: btoa(content), project: undefined, file: undefined, line: undefined});
	}
}

function editOutput() {
	params.update({ file: null, content: null });
	setContent(terminal.text());
}

function shareInput() {
	setStyle(document.body, 'output');
	let content = editor.getValue();
	terminal.print('decoded uri: ' + decodeURIComponent(window.location));
	terminal.print('current file: ' + window.location.origin + window.location.pathname + '#content=' + btoa(content));
	if (content.startsWith('[{')) {
		terminal.print('project file: ' + window.location.origin + window.location.pathname + '#project=' + btoa(content));
	}
}

function execute(cmd, exactArgs) {
	let args = undefined;
	if (typeof(exactArgs) !== 'object') {
		exactArgs = {
			addFile: !exactArgs,
			addLibs: !exactArgs,
			addPrms: !exactArgs
		}
	}

	if (cmd != null) {
		let file = undefined;
		if (exactArgs.addFile) {
			file = saveInput();
			if (file == null) {
				return;
			}
		}

		args = [];
		for (let arg of cmd.split(' ')) {
			if (arg === '') {
				continue;
			}
			args.push(arg);
		}

		if (exactArgs.addPrms) {
			// do not use standard input, print times
			args.push('-X' + (params.X || '-stdin+steps'));

			// allocate 2Mb of memory by default,
			args.push('-mem' + (params.mem || '2M'));

			if (params.dump != null) {
				if (params.dump.endsWith('.json')) {
					args.push('-dump.json');
				} else {
					args.push('-dump');
				}
				args.push(params.dump);
			}

			if (params.log != null) {
				args.push('-log');
				args.push(params.log);
			}
		}

		if (exactArgs.addLibs) {
			// todo: used libraries should be defined in the project file
			for (let lib of props.libraries) {
				args.push(lib);
			}
		}

		if (file !== undefined) {
			args.push(file);
			for (let ln of document.getElementsByClassName('breakpoint')) {
				args.push('-b/P/' + ln.value);
			}
		}
		terminal.command = args.join(' ');
	}

	terminal.clear();
	setStyle(document.body, 'output');
	execInput(args || ['--help']);
}
