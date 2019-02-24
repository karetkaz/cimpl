let docTitle = document.title;
let editor = CodeMirror.fromTextArea(input, {
	mode: "text/x-cmpl",
	lineNumbers: true,
	tabSize: 4,
	indentUnit: 4,
	indentWithTabs: true,

	styleActiveLine: true,
	matchBrackets: true,
	showTrailingSpace: true,

	foldGutter: true,
	gutters: ["CodeMirror-linenumbers", "CodeMirror-foldgutter"],
	highlightSelectionMatches: {showToken: /\w/, annotateScrollbar: true}
});
let terminal = Terminal(output, function(escaped, text) {
	const regex = /(?=[\w\/])((?:\w+:\/\/)(?:[^:@\n\r"'`]+(?:\:\w+)?@)?(?:[^:/?#\n\r"'`]+)(?:\:\d+)?(?=[/?#]))?([^<>=:;,?#*|\n\r"'`]*)?(?:\:(?:(\d+)(?:\:(\d+))?))?(?:\?([^#\n\r"'`]*))?(?:\#([^\n\r"'`]*))?/g;
	return escaped.replace(regex, function (match, host, path, line, column, query, hash) {
		if (path === undefined) {
			return match;
		}

		line = +(line || 0);
		column = +(column || 0);
		if (host !== undefined) {
			return '<a href="' + encodeURI(match) + '" target="_blank">' + match + '</a>';
		}
		if (line > 0) {
			return '<a href="#" onClick="return showFile(\'' + path + '\',' + line + ',' + column + ');">' + match + '</a>';
		}
		return match;
	});
});
let params = JsArgs('#', function (params, changes) {
	//console.trace('params: ', changes, params);
	// setup execution method
	if (!changes || changes.exec != null) {
		if (params.exec == null) {
			btnExecute.value = '-run/g';
			btnExecute.innerText = 'Run';
		} else {
			btnExecute.value = params.exec;
			btnExecute.innerText = 'Execute';
		}
	}

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

	// set editor content
	if (!changes || changes.content) {
		let content = params.content;

		try {
			if (content != null) {
				content = atob(content);
			}
		} catch (e) {
			console.warn(e);
		}

		editor.setValue(content || '');
	}

	// setup filename
	if (params.file != null) {
		document.title = params.file + " - " + docTitle;
		edtFileName.value = params.file;
	} else {
		document.title = docTitle;
		edtFileName.value = '';
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

	// file changed and there is no content -> (open or close)
	if (changes.file && !params.content) {
		if (params.file != null) {
			openInput(params.file, params.line);
		} else {
			editor.setValue('');
		}
	}

	if (changes.line && params.line != null) {
		editor.setCursor(params.line - 1);
	}
});

function actionError() {
	edtFileName.classList.add('error');
	setTimeout(function() {
		edtFileName.classList.remove('error');
	}, 250);
	return false;
}

editor.setSize('100%', '100%');

edtFileName.onfocus = function() {
	edtFileName.selectionStart = 0;
	edtFileName.selectionEnd = edtFileName.value.length;
}
edtFileName.onblur = function() {
	edtFileName.value = params.file || '';
	editor.focus();
}

function completeAction(event) {
	if (edtFileName.value !== event.key) {
		return;
	}
	if (event.key === ':' || event.key === '?' || event.key === '!') {
		edtFileName.value = event.key + (event.text || '');
		edtFileName.selectionStart = 1;
		edtFileName.selectionEnd = edtFileName.value.length;
		return;
	}
	if (event.key === '#') {
		edtFileName.value = event.key + (params.exec || btnExecute.value);
		edtFileName.selectionStart = 1;
		edtFileName.selectionEnd = edtFileName.value.length;
		return;
	}
}
edtFileName.onkeyup = completeAction;
edtFileName.onkeydown = function(event) {
	if (event.key === 'Escape') {
		edtFileName.blur();
		return false;
	}
	if (event.key !== 'Enter') {
		return true;
	}

	// `#arguments` => execute script
	if (edtFileName.value.startsWith('#')) {
		let command = edtFileName.value.substr(1);
		execute('Execute', command);
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
		showFile(null);
	}
	// open file
	else {
		showFile(edtFileName.value);
	}
	edtFileName.blur();
	return false;
}

edtArguments.onkeypress = function() {
	if (event.key !== 'Enter') {
		return;
	}
	execute('Execute', ' ' + edtArguments.value);
}

window.onkeydown = function() {
	if (event.metaKey) {
		return true;
	}

	if (event.ctrlKey && !event.altKey && event.shiftKey && event.key === '-') {
		editor.execCommand('foldAll');
		return false;
	}

	if (event.ctrlKey && !event.altKey && event.shiftKey && event.key === '_') {
		editor.execCommand('foldAll');
		return false;
	}

	if (event.ctrlKey && !event.altKey && event.shiftKey && event.key === '+') {
		editor.execCommand('unfoldAll');
		return false;
	}
	if (event.ctrlKey && !event.altKey && event.shiftKey && event.key === '=') {
		editor.execCommand('unfoldAll');
		return false;
	}

	// Ctrl + s => save script
	if (event.ctrlKey && !event.altKey && !event.shiftKey && event.key === 's') {
		saveInput();
		return false;
	}

	// Ctrl + f => search
	if (event.ctrlKey && !event.altKey && !event.shiftKey && event.key === 'f') {
		edtFileName.focus();
		edtFileName.value = '?';
		completeAction({key: '?', text: editor.getSelection() || 'enter a text to search'});
		return false;
	}

	// Ctrl + r => search
	if (event.ctrlKey && !event.altKey && !event.shiftKey && event.key === 'r') {
		edtFileName.focus();
		edtFileName.value = '!';
		completeAction({key: '!', text: editor.getSelection() || 'enter a text to replace'});
		return false;
	}

	// Ctrl + g => goto line
	if (event.ctrlKey && !event.altKey && !event.shiftKey && event.key === 'g') {
		edtFileName.focus();
		edtFileName.value = ':';
		completeAction({key: ':', text: editor.getCursor().line || 'enter a line number to go to'});
		return false;
	}

	if (!event.ctrlKey && !event.altKey && !event.shiftKey && event.key === 'F3') {
		let cursor = edtFileName.cursor;
		if (cursor && cursor.findNext()) {
			editor.setSelection(cursor.from(), cursor.to());
		} else {
			actionError();
		}
		return false;
	}
	if (!event.ctrlKey && !event.altKey && event.shiftKey && event.key === 'F3') {
		let cursor = edtFileName.cursor;
		if (cursor && cursor.findPrevious()) {
			editor.setSelection(cursor.from(), cursor.to());
		} else {
			actionError();
		}
		return false;
	}

	// Ctrl + Shift + Enter => Execute script
	if (event.ctrlKey && !event.altKey && event.shiftKey && event.key === 'Enter') {
		btnExecute.click();
		return false;
	}

	// Ctrl + Enter => search, jump, fold, ...
	if (event.ctrlKey && !event.altKey && !event.shiftKey && event.key === 'Enter') {
		edtFileName.focus();
		return false;
	}
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

function showFile(file, line, column) {
	showEditor('-secondary');
	editor.setCursor((line || 1) - 1, column);
	if (file == null || file !== params.file) {
		params.update({
			file: file,
			line: line,
			content: null	// when navigating to a different file remove content
		});
	}
	return false;
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
	editor.setValue(terminal.text());
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

function execute(text, cmd) {
	let exactArgs = cmd && cmd.startsWith(' ');
	let args = undefined;
	let file = undefined;

	if (text != null) {
		btnExecute.innerText = text;
	}
	if (cmd != null) {
		btnExecute.value = cmd;

		args = [];
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
		}

		for (let arg of cmd.split(' ')) {
			if (arg === '') {
				continue;
			}
			args.push(arg);
		}
	}

	terminal.clear();
	showEditor('-primary');
	execInput(args, exactArgs);
}
