var props = props || {};
props.title = document.title;
const pathMather = /(?=[\w\/])((?:\w+:\/\/)(?:[^:@\n\r"'`]+(?:\:\w+)?@)?(?:[^:/?#\n\r"'`]+)(?:\:\d+)?(?=[/?#]))?([^<>=:;,?#*|\n\r"'`]*)?(?:\:(?:(\d+)(?:\:(\d+))?))?(?:\?([^#\n\r"'`]*))?(?:\#([^\n\r"'`]*))?/;
const pathFinder = new RegExp(pathMather, 'g');

CodeMirror.commands.save = function(cm) {
	saveInput();
}

CodeMirror.commands.line = function(cm) {
	completeAction({key: ':', valueHint: 'enter a line number to go to'});
}

CodeMirror.commands.find = function(cm) {
	completeAction({key: '?', valueHint: 'enter a text to search'});
}

CodeMirror.commands.replace = function(cm) {
	completeAction({key: '!', valueHint: 'enter a text to replace'});
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

	// setup theme, only after loading
	if (!changes && params.theme != null) {
		setTheme(document.body, params.theme || 'dark', 'dark', 'light');
	}

	// open menu, only after loading
	if (!changes && params.menu != null) {
		showSplitter(window.menuSplit, params.menu === 'none' ? 'secondary' : params.menu);
	}

	// show editor or output, only after loading
	if (!changes && params.split != null) {
		showEditor(params.split);
	}

	// setup project, only after loading
	if (!changes && params.project != null) {
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
			for (let file of content.split(';')) {
				files.push({
					path: file.replace(/^(.*[/])?(.*)(\..*)$/, "$2$3"),
					url: file
				});
			}
		}
		openProject(files);
	}

	// setup editor content, only after loading
	if (!changes && params.content != null) {
		let content = params.content;
		try {
			content = atob(content);
		} catch (e) {
			console.warn(e);
		}
		setContent(content, params.file, params.line);
	}

	if (changes === undefined) {
		// wait for onRuntimeInitialized
		return;
	}

	// reload page if the project was modified
	if (changes.project) {
		params.update(true);
		return;
	}

	if (changes.file) {
		if (params.file == null && params.content == null) {
			// close the current file
			setContent('', '');
			return;
		}
		openInput(params.file, params.line);
		return;
	}

	if (changes.line) {
		setContent(undefined, params.file, params.line);
	}

	if (changes.content) {
		let content = params.content;
		if (content != null) {
			try {
				content = atob(content);
			} catch (e) {
				console.warn(e);
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

editor.focus();
editor.setSize('100%', '100%');

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
		execute(command);
		params.update({exec: command});
	}

	// `:<number>` => goto line
	else if (edtFileName.value.startsWith(':')) {
		let line = edtFileName.value.substr(1);
		if (isNaN(line)) {
			return actionError();
		}
		params.update({line: line});
	}

	// !find text
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

	// !replace all occurences
	else if (edtFileName.value.startsWith('!')) {
		let text = edtFileName.value.substr(1);
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

	// `%` => zoom
	else if (edtFileName.value.startsWith('%')) {
		document.body.style.fontSize = (+edtFileName.value.substr(1) / 100) + 'em';
		editor.setSize('100%', '100%');
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

		let file = match[2].replace(/^(.*[/])?(.*)(\..*)$/, "$2$3");
		let line = match[3];
		if (match[1] != null) {
			openProject([{ url: match[1] + match[2], path: file }]);
		}
		params.update({ content: null, file, line: match[3] });
	}

	edtFileName.blur();
	return false;
}
edtFileName.onkeyup = completeAction;

edtExecute.onkeydown = function(event) {
	if (event.key !== 'Enter') {
		return true;
	}
	if (edtExecute.value === '') {
		execute((params.exec || '-run/g'));
	} else {
		execute(edtExecute.value, true);
	}
	return false;
}

window.onkeydown = function() {
	// escape to editor
	if (event.key == 'Escape') {
		editor.setSelection(editor.getCursor());
		editor.focus();
		return false;
	}
	// Ctrl + Shift + Enter => Execute script
	if (event.ctrlKey && !event.altKey && event.shiftKey && event.key === 'Enter') {
		//edtExecute.onkeydown(event);
		edtExecute.focus();
		return false;
	}
	// Ctrl + Enter => Execute command: [search, jump, fold, run, ...]
	if (event.ctrlKey && !event.altKey && !event.shiftKey && event.key === 'Enter') {
		edtFileName.focus();
		return false;
	}
}

function completeAction(event) {
	if (event.valueHint !== undefined) {
		edtFileName.value = event.key;
		edtFileName.focus();
	}
	if (edtFileName.value !== event.key) {
		return;
	}

	switch (event.key) {
		case ':':
			edtFileName.value += editor.getCursor().line + 1 || event.valueHint || '';
			break;

		case '?':
		case '!':
			edtFileName.value += editor.getSelection() || event.valueHint || '';
			break;

		case '#':
			edtFileName.value += params.exec || '';
			break;
	}
	edtFileName.selectionStart = 1;
	edtFileName.selectionEnd = edtFileName.value.length;
}

function setTheme(element, theme, ...remove) {
	if (theme == null) {
		return;
	}

	let toggle = false;
	if (theme.startsWith('!')) {
		theme = theme.substring(1);
		toggle = true;
	}

	element.classList.remove(...remove);
	if (toggle && element.classList.contains(theme)) {
		element.classList.remove(theme);
	} else {
		element.classList.add(theme);
	}
}

function showEditor(...options) {
	showSplitter(window.editSplit, ...options);
	editor.setSize('100%', '100%');
}

function setContent(content, file, line, column) {
	showEditor('-secondary');
	let contentSet = false;
	if (content != null && content != editor.getValue()) {
		editor.setValue(content);
		contentSet = true;
	}
	if (file != null) {
		edtFileName.value = file;
	}
	if (line != null) {
		var middleHeight = editor.getScrollerElement().offsetHeight / 2; 
		var t = editor.charCoords({line, ch: 0}, "local").top; 
		editor.scrollTo(null, t - middleHeight - 5);
		editor.setCursor((line || 1) - 1, column);
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
			for (let file of content.split(';')) {
				files.push({
					path: file.replace(/^(.*[\/])?(.*)(\..*)$/, "$2$3"),
					url: file
				});
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
	showEditor('-primary');
	let content = editor.getValue();
	terminal.print('decoded uri: ' + decodeURIComponent(window.location));
	if (content.startsWith('[{')) {
		terminal.print('project file: ' + window.location.origin + window.location.pathname + '#project=' + btoa(content));
	}
	terminal.print('current file: ' + window.location.origin + window.location.pathname + '#content=' + btoa(content));
}

function execute(cmd, exactArgs) {
	let args = undefined;
	let file = saveInput();

	if (file == null) {
		return;
	}
	if (cmd != null) {
		args = [];
		for (let arg of cmd.split(' ')) {
			if (arg === '') {
				continue;
			}
			args.push(arg);
		}

		if (exactArgs !== true) {
			// do not use standard input, print times
			args.push('-X' + (params.X || '-stdin+steps'));

			// allocate 2Mb of memory by default,
			args.push('-mem' + (params.mem || '2M'));

			// standard library is in root
			args.push('-std/lib/stdlib.ci');

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

			// todo: used libraries should be defined in the project file
			for (let lib of props.libraries) {
				args.push(lib);
			}

			args.push(file);
			for (let ln of document.getElementsByClassName('breakpoint')) {
				args.push('-b/P/' + ln.value);
			}

		}
		edtExecute.value = args.join(' ');
	}

	terminal.clear();
	showEditor('-primary');
	execInput(args || ['--help']);
}
