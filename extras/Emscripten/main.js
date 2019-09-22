function setStyle(splitter, ...styles) {
	if (splitter && splitter.constructor === String) {
		// allow to reference the splitter using id.
		splitter = document.getElementById(splitter);
	}

	let cycleIndex = 0;
	let cycleItems = [];
	for (let style of styles) {

		// remove style
		if (style.startsWith('-')) {
			splitter.classList.remove(style.substring(1));
			continue;
		}

		// toggle style
		if (style.startsWith('!')) {
			style = style.substring(1);
			if (splitter.classList.contains(style)) {
				splitter.classList.remove(style);
				continue;
			}
		}

		// cycle style
		if (style.startsWith('^')) {
			style = style.substring(1);
			cycleItems.push(style);
			if (splitter.classList.contains(style)) {
				splitter.classList.remove(style);
				cycleIndex = cycleItems.length;
			}
			continue;
		}

		splitter.classList.add(style);
	}
	if (cycleItems.length > 0) {
		let style = cycleItems[cycleIndex % cycleItems.length];
		if (style !== '') {
			splitter.classList.add(style);
		}
	}
	editor.refresh();
}


var props = props || {};
props.title = document.title;
const pathMather = /(?=[\w\/])((?:\w+:\/\/)(?:[^:@\n\r"'`]+(?:\:\w+)?@)?(?:[^:/?#\n\r"'`]+)(?:\:\d+)?(?=[/?#]))?([^<>=:;,?#*|\n\r"'`]*)?(?:\:(?:(\d+)(?:\:(\d+))?))?(?:\?([^#\n\r"'`]*))?(?:\#([^\n\r"'`]*))?/;
const pathFinder = new RegExp(pathMather, 'g');

CodeMirror.commands.save = function(cm) {
	saveInput();
}
CodeMirror.commands.line = function(cm) {
	completeAction({key: ':', valueText: ':'});
}

CodeMirror.commands.find = function(cm) {
	completeAction({key: '?', valueText: '?'});
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
CodeMirror.commands.findAndSelect = function(cm) {
	completeAction({key: '?!', valueText: '?!'});
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
				console.warn(e);
			}
			setContent(content, params.file, params.line);
		}
		return;
	}

	// reload page if the project was modified
	let files = [];
	if (changes.project !== undefined) {
		if (changes.project != params.project) {
			params.update(true);
			return;
		}

		let content = params.project;

		try {
			content = atob(content);
		} catch (e) {
			console.warn(e);
		}

		if (content != null) {
			if (content.startsWith('[{')) {
				files = JSON.parse(content);
			} else {
				for (let url of content.split(';')) {
					files.push({ url });
				}
			}
		}
		/*openProjectFile({
			workspace: params.workspace,
			file: params.file,
			line: params.line,
			list: files
		});
		return;*/
		if (changes.workspace === undefined) {
			changes.workspace = null;
		}
	}

	if (changes.workspace !== undefined) {
		openProjectFile({
			workspace: params.workspace,
			file: params.file,
			line: params.line,
			list: files
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
		//params.update({exec: command});
		execute(command, true);
	}

	// `:%` => zoom
	else if (edtFileName.value.startsWith(':%')) {
		document.body.style.fontSize = (+edtFileName.value.substr(2) / 100) + 'em';
		editor.setSize('100%', '100%');
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
			list: JSON.parse(edtFileName.value),
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
			openProjectFile({ file, list: [{
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
		editor.setSelection(editor.getCursor());
		if (!editor.hasFocus()) {
			setStyle(document.body, 'editor');
		} else {
			setStyle(document.body, '!output');
		}
		editor.focus();
		return false;
	}
	// Ctrl + Enter => Execute script
	if (event.ctrlKey && !event.altKey && !event.shiftKey && event.key === 'Enter') {
		setStyle(document.body, 'output');
		execute((params.exec || '-run/g'));
		return false;
	}
	// Shift + Enter => Focus command: [search, jump, fold, run, ...]
	if (!event.ctrlKey && !event.altKey && event.shiftKey && event.key === 'Enter') {
		edtFileName.focus();
		return false;
	}
	// Ctrl + Shift + Enter => Execute script
	if (event.ctrlKey && !event.altKey && event.shiftKey && event.key === 'Enter') {
		completeAction({key: '#', valueText: '#'});
		return false;
	}
}

function completeAction(event) {
	if (event.valueText !== undefined) {
		edtFileName.value = event.valueText;
		edtFileName.focus();
	}

	if (!edtFileName.value.endsWith(event.key)) {
		// maybe backspace was pressed
		return;
	}

	switch (edtFileName.value) {
		default:
			return;

		case ':':
			edtFileName.value += editor.getCursor().line + 1 || 'enter a line number to go to';
			edtFileName.selectionStart = 1;
			break;

		case '?':
			edtFileName.value += editor.getSelection() || 'enter a text to search';
			edtFileName.selectionStart = 1;
			break;

		case '?!':
			edtFileName.value += editor.getSelection() || 'enter a text to search and select occurrences';
			edtFileName.selectionStart = 2;
			break;

		case '#':
			edtFileName.value += selExecute.command || '';
			edtFileName.selectionStart = 1;
			break;
	}
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
			li.classList.remove("active");
			if (li.innerText === file) {
				li.classList.add("active");
			}
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
		selExecute.command = args.join(' ');
	}

	terminal.clear();
	setStyle(document.body, 'output');
	execInput(args || ['--help']);
}
