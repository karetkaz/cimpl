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

	function content(value) {
		if (value != null) {
			try {
				return atob(value);
			} catch (e) {
				console.debug(e);
			}
		}
		return value;
	}
	function project(value) {
		value = content(value);
		if (value != null) {
			if (value.startsWith('[{')) {
				return JSON.parse(value);
			}

			let result = [];
			for (let url of value.split(';')) {
				result.push({ url });
			}
			return result;
		}
		return value;
	}

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

		if (params.project != null) {
			setStyle(projectOptions, '-hidden');
		}
		if (params.workspace != null) {
			workspaceName.innerText = ': ' + params.workspace;
		}

		// setup editor content, only after loading
		if (params.content != null) {
			setContent(content(params.content), params.file, params.line);
		}

		// custom layout, only after loading
		switch (params.show || 'auto') {
			default:
			case 'auto':
				if (!('ontouchstart' in document.documentElement)) {
					setStyle(document.body, 'editor', 'left-pin', 'left-bar');
					editor.setSize('100%', '100%');
					break;
				}
				// fall trough: using mobile

			case 'mobile':
				if (params.content == null && params.file == null) {
					setStyle(document.body, 'editor', 'left-bar');
				} else {
					setStyle(document.body, 'editor');
				}
				document.body.style.fontSize = '1.2em';
				editor.setSize('100%', '100%');
				break;

			case "embedded":
				setStyle(document.body, 'autoheight', 'editor');
				editor.setOption("viewportMargin", Infinity);
				break;

			case 'editor':
				setStyle(document.body, 'editor');
				editor.setSize('100%', '100%');
				break;

			case 'normal':
				setStyle(document.body, 'editor', 'left-pin', 'left-bar');
				editor.setSize('100%', '100%');
				break;

			case 'full':
				setStyle(document.body, 'editor', 'output', 'left-pin', 'left-bar', 'bottom-bar');
				editor.setSize('100%', '100%');
				break;
		}
		editor.focus();

		if (params.workspace != null || params.project != null) {
			// do not show workspaces if a project or workspace is loaded
			workspaceList.innerHTML = '';
			return;
		}
		if (indexedDB.databases == null) {
			terminal.print('Failed to list workspaces, indexedDB.databases not available');
			return;
		}
		workspaceName.innerText = 's';
		indexedDB.databases().then(function(dbs) {
			let prefix = '/cmpl/';
			for (db of dbs) {
				let name = db.name;
				if (!name.startsWith(prefix)) {
					continue;
				}
				name = name.substr(prefix.length);
				let url = window.location.origin + window.location.pathname + '#workspace=' + name;
				workspaceList.innerHTML += '<li onclick="params.update({workspace: \'' + name + '\'});">' + 'Workspace: ' + name +
					'<button class="right" onclick="event.stopPropagation(); rmWorkspace(\'' + name + '\')" title="Remove workspace">-</button>' +
					'</li>'
			}
		});
		return;
	}

	// reload page if the project was modified
	if (changes.project !== undefined) {
		if (changes.project != params.project) {
			// page needs to be reloaded to clean up old files
			// keep the workspace clean, only one project
			params.update(true);
			return;
		}
		if (changes.workspace === undefined) {
			changes.workspace = true;
		}
	}

	if (changes.workspace !== undefined) {
		if (changes.workspace != params.workspace) {
			// page needs to be reloaded to clean up old files
			// mounting one folder second time will fail
			params.update(true);
			return;
		}
		openProjectFile({
			workspace: params.workspace,
			project: project(params.project),
			content: content(params.content),
			folder: params.folder,
			file: params.file,
			line: params.line,
		});
		if (params.workspace) {
			params.project = null;
			params.content = null;
			params.folder = null;
			params.file = null;
			params.line = null;
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
		setContent(content(params.content), params.file, params.line);
	}
});

function rmWorkspace(workspace) {
	if (!confirm("Remove workspace: " + workspace)) {
		return;
	}
	if (!workspace.startsWith('/cmpl/')) {
		workspace = '/cmpl/' + workspace;
	}
	var req = indexedDB.deleteDatabase(workspace);
	req.onsuccess = function(event) {
		console.log(arguments);
		if (event.oldVersion === 0) {
			return;
		}
		params.update(true);
	}
	req.onerror = console.log;
	req.onblocked = console.log;
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
		setStyle(document.body, 'editor');
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
	execInput(args || ['--help']);
}

function process(data) {
	if (data == null) {
		return;
	}
	if (data.error !== undefined) {
		terminal.print(data.error);
		actionError();
	}
	if (data.print !== undefined) {
		terminal.print(data.print);
	}
	if (data.list !== undefined) {
		fileList.innerHTML = '';
		for (let file of data.list) {
			if (file.endsWith('/')) {
				fileList.innerHTML += '<li onclick="params.update({folder: this.innerText});">' + file + '</li>';
			}
			else if (file === params.file) {
				fileList.innerHTML += '<li class="active" onclick="params.update({file: this.innerText, line: null});">' + file + '</li>';
			}
			else {
				fileList.innerHTML += '<li onclick="params.update({file: this.innerText, line: null});">' + file + '</li>';
			}
		}
	}

	if (data.content || data.file || data.line) {
		if (setContent(data.content, data.file, data.line)) {
			params.update({
				file: data.file || params.file,
				line: data.line || params.line,
				content: params.content ? data.content : null
			});
		} else {
			params.update({
				file: data.file,
				line: data.line
			});
		}
	}
	if (data.initialized) {
		params.update('workspace', 'project', 'content', 'folder', 'file');
	}
}