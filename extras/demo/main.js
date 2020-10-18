function hasStyle(element, ...styles) {
	for (let style of styles) {
		if (element.classList.contains(style)) {
			return true;
		}
	}
	return false;
}
function setStyle(element, ...styles) {
	if (element && element.constructor === String) {
		// allow to reference the splitter using id.
		element = document.getElementById(element);
	}

	let prevChanged = false;
	for (let style of styles) {
		// conditional apply
		if (style.startsWith('?')) {
			if (!prevChanged) {
				continue;
			}
			style = style.substring(1);
		}

		prevChanged = false;
		// remove style
		if (style.startsWith('-')) {
			style = style.substring(1);
			if (element.classList.contains(style)) {
				element.classList.remove(style);
				prevChanged = true;
			}
			continue;
		}

		// toggle style
		if (style.startsWith('~')) {
			style = style.substring(1);
			if (element.classList.contains(style)) {
				element.classList.remove(style);
			} else {
				element.classList.add(style);
			}
			prevChanged = true;
			continue;
		}

		if (element.classList.contains(style)) {
			// style already set
			continue;
		}
		element.classList.add(style);
		prevChanged = true;
	}

	if (element === document.body) {
		editor.refresh();
	}
	return prevChanged;
}

var props = props || {};
props.title = document.title;
const pathMather = /(?=[\w\/])((?:\w+:\/\/)(?:[^:@\n\r"'`]+(?:\:\w+)?@)?(?:[^:/?#\n\r"'`]+)(?:\:\d+)?(?=[/?#]))?([^<>=:;,?#*|\n\r"'`]*)?(?:\:(?:(\d+)(?:\:(\d+))?))?(?:\?([^#\n\r"'`]*))?(?:\#([^\n\r"'`]*))?/;
const pathFinder = new RegExp(pathMather, 'g');

let editor = CodeMirror.fromTextArea(input, {
	mode: 'text/x-cmpl',
	lineNumbers: true,
	tabSize: 4,
	indentUnit: 4,
	indentWithTabs: true,
	keyMap: 'extraKeys',

	styleActiveLine: true,
	matchBrackets: true,
	showTrailingSpace: true,

	foldGutter: true,
	gutters: ['CodeMirror-linenumbers', 'breakpoints', 'CodeMirror-foldgutter'],
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
		let theme = undefined;
		let mode = undefined;
		if (params.theme != null) {
			switch (params.theme) {
				case 'light':
				case 'dark':
					theme = params.theme;
					// mode = undefined;
					break;

				case 'embedded-light':
					theme = 'light';
					mode = 'embedded';
					break;

				case 'embedded-dark':
					theme = 'dark';
					mode = 'embedded';
					break;

				case 'embedded':
				case 'normal':
				case 'mobile':
				case 'dbg':
					// theme = undefined;
					mode = params.theme;
					break;
			}
		}

		// theme is not defined, try to guess it
		if (theme === undefined) {
			if (document.body.classList.contains('dark')) {
				theme = 'dark';
			}
			else if (document.body.classList.contains('light')) {
				theme = 'light';
			}
			else if (window.matchMedia) {
				// theme not overridden by page, lookup system theme
				if (window.matchMedia('(prefers-color-scheme: dark)').matches) {
					theme = 'dark';
				}
				else if (window.matchMedia('(prefers-color-scheme: light)').matches) {
					theme = 'light';
				}
			}
		}

		// mode is not defined, try to guess it
		if (mode === undefined) {
			if ('ontouchstart' in document.documentElement) {
				mode = 'mobile';
			} else {
				mode = 'normal';
			}
		}

		setStyle(document.body, '-dark', '-light', theme || 'dark');

		// custom layout, only after loading
		props.mobile = mode === 'mobile';
		switch (mode) {
			case 'normal':
				if (params.workspace != null || params.project != null) {
					setStyle(document.body, 'output');
				} else {
					setStyle(document.body, '-output');
				}
				editor.setSize('100%', '100%');
				editor.focus();
				break;

			case 'mobile':
				// for mobile show the left menu, in case there is no content loaded
				if (params.content != null || params.file != null) {
					setStyle(document.body, '-left-pin', '-left-bar', '-output');
				} else {
					setStyle(document.body, '-left-pin', 'left-bar', '-output');
				}
				document.body.style.fontSize = '1.2em';
				editor.setSize('100%', '100%');
				editor.focus();
				break;

			case 'embedded':
				setStyle(document.body, 'autoheight', '-left-bar', '-output');
				editor.setOption('viewportMargin', Infinity);
				break;

			case 'dbg':
				setStyle(document.body, 'output', 'left-pin', 'left-bar', 'right-pin', 'right-bar', 'bottom-pin', 'bottom-bar');
				editor.setSize('100%', '100%');
				editor.focus();
				break;
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
		return;
	}

	if (changes.theme !== undefined) {
		setStyle(document.body, '-dark', '-light', params.theme);
	}

	if (changes.worker !== undefined) {
		// page needs to be reloaded to apply different javascript
		params.update(true);
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
		openProjectFile({
			folder: params.folder || '~/'
		})
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
	if (!confirm('Remove workspace: ' + workspace)) {
		return;
	}
	if (!workspace.startsWith('/cmpl/')) {
		workspace = '/cmpl/' + workspace;
	}
	var req = indexedDB.deleteDatabase(workspace);
	req.onsuccess = function(event) {
		if (event.oldVersion === 0) {
			return;
		}
		params.update(true);
	}
	req.onerror = console.log;
	req.onblocked = console.log;
}

editor.on('change', function() {
	setStyle(document.body, 'edited');
});
editor.on('gutterClick', function(cm, n) {
	let info = cm.lineInfo(n);
	if (info.gutterMarkers == null) {
		let marker = document.createElement('div');
		marker.classList.add('breakpoint');
		marker.innerHTML = '●';
		marker.value = n + 1;
		cm.setGutterMarker(n, 'breakpoints', marker);
	} else {
		cm.setGutterMarker(n, 'breakpoints', null);
	}
});

window.onkeydown = function() {
	// escape to editor
	if (event.key == 'Escape') {
		setStyle(document.body, '-right-bar');
		if (editor.hasFocus()) {
			setStyle(document.body, '~output');
		}
		if (!document.body.classList.contains('canvas')) {
			editor.setSelection(editor.getCursor());
			editor.focus();
		}
		return false;
	}
	// Ctrl + Enter => Focus command: [search, jump, fold, run, ...]
	if (event.ctrlKey && !event.altKey && !event.shiftKey && event.key === 'Enter') {
		completeAction();
		return false;
	}
	// Ctrl + Shift + Enter => Execute script
	if (event.ctrlKey && !event.altKey && event.shiftKey && event.key === 'Enter') {
		onExecClick();
		return false;
	}
}

function setContent(content, file, line, column) {
	let contentSet = false;
	if (content != null && content != editor.getValue()) {
		editor.setValue(content);
		editor.clearHistory();
		setStyle(document.body, '-edited');
		contentSet = true;
	}
	if (file != null) {
		edtFileName.value = file;
		for (let li of fileList.children) {
			setStyle(li, li.innerText === file ? 'active' : '-active');
		}
	}
	if (line != null) {
		var middleHeight = editor.getScrollerElement().offsetHeight / 2;
		var t = editor.charCoords({line, ch: 0}, 'local').top;
		editor.scrollTo(null, t - middleHeight - 5);
		editor.setCursor((line || 1) - 1, column);
	}
	return contentSet;
}

function hideOverlay() {
	if (!document.body.classList.contains('left-pin')) {
		document.body.classList.remove('left-bar');
	}
	if (!document.body.classList.contains('right-pin')) {
		document.body.classList.remove('right-bar');
	}
	if (!document.body.classList.contains('bottom-pin')) {
		document.body.classList.remove('bottom-bar');
	}
}

function saveInput(saveAs) {
	let file = params.file;
	if (file == null || saveAs === true) {
		file = prompt('Save file as:', file || 'untitled.ci');
		if (file == null) {
			return null;
		}
	}
	let content = editor.getValue();
	openProjectFile({ content, file });
	setStyle(document.body, '-edited');
	if (params.content !== undefined) {
		let oldValue = params.content;
		let encB64 = true;
		try {
			oldValue = atob(oldValue);
		} catch (err) {
			encB64 = false;
			console.debug(err);
		}
		if (oldValue !== content) {
			params.update({ content: encB64 ? btoa(content) : content });
		} else {
			params.update({ file });
		}
	} else {
		params.update({ file });
	}
	return file;
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
				content += ', ';
			}
			content += JSON.stringify(file, null, '\t');
		}
		content = '[' + content + ']';
		params.update({content: btoa(content), project: undefined, file: undefined, line: undefined});
	}
}

function selectOutputTab(select) {
	let item = select.options[select.selectedIndex];
	if (item.onchange != null) {
		if (item.keepSelected !== true) {
			select.selectedIndex = 0;
		}
		item.onchange();
	}
}

function pinOutput(pinOption) {
	let newOption = document.createElement('option');
	newOption.value = terminal.innerHtml();
	pinOption.value = +(+pinOption.value || 0) + 1;
	newOption.innerText = 'Output-' + pinOption.value;
	newOption.keepSelected = true;
	newOption.onchange = function () {
		terminal.innerHtml(newOption.value);
	}
	selOutput.insertBefore(newOption, pinOption);
	selOutput.selectedIndex = Array.prototype.indexOf.call(selOutput.children, newOption);
}

function editOutput() {
	params.update({ file: null, content: null });
	setContent(terminal.text());
	if (props.mobile) {
		setStyle(document.body, '-output');
	}
}

function shareInput() {
	setStyle(document.body, 'output');
	let content = editor.getValue();
	terminal.append('decoded uri: ' + decodeURIComponent(window.location));
	let hash = 'content=' + btoa(editor.getValue());
	if (params.file !== null) {
		hash = 'file=' + params.file + '&' + hash;
	}
	terminal.append('current file: ' + window.location.origin + window.location.pathname + '#' + hash);
	if (content.startsWith('[{')) {
		terminal.append('project file: ' + window.location.origin + window.location.pathname + '#project=' + btoa(content));
	}
}

function execute(cmd, args) {
	let execArgs = undefined;
	if (typeof(args) !== 'object') {
		args = {
			file: args !== false,
			libs: args !== false,
			prms: args !== false,
			dump: false
		}
	}

	if (cmd != null) {

		execArgs = [];
		for (let arg of cmd.split(' ')) {
			if (arg === '') {
				continue;
			}
			execArgs.push(arg);
		}

		if (args.prms !== false) {
			// do not use standard input, print times
			execArgs.push('-X' + (params.X || '-stdin+steps'));

			// allocate 2Mb of memory by default,
			execArgs.push('-mem' + (params.mem || '2M'));

			if (params.log != null) {
				execArgs.push('-log');
				execArgs.push(params.log);
			}
		}

		if (args.dump !== false && args.dump != null) {
			if (args.dump.constructor !== String) {
				throw 'dump must be a string';
			}
			if (args.dump.endsWith('.json')) {
				execArgs.push('-dump.json');
			} else {
				execArgs.push('-dump');
			}
			execArgs.push(args.dump);
		}

		if (args.libs !== false) {
			if (args.libs === true || args.libs == null) {
				args.libs = props.libraries;
			}
			// todo: used libraries should be defined in the project file
			for (let lib of args.libs) {
				execArgs.push(lib);
			}
		}

		if (args.file !== false) {
			args.file = saveInput();
			if (args.file == null) {
				return;
			}

			execArgs.push(args.file);
			for (let ln of document.getElementsByClassName('breakpoint')) {
				execArgs.push('-b/P/' + ln.value);
			}
		}

		terminal.command = execArgs.join(' ');
	}

	terminal.clear();
	selOutput.selectedIndex = 0;
	execInput(execArgs || ['--help'], args);
}

function process(data) {
	if (data == null) {
		return;
	}
	if (data.error !== undefined) {
		terminal.append(data.error);
		actionError();
	}
	if (data.print !== undefined) {
		terminal.append(data.print);
	}
	if (data.link !== undefined) {
		const link = document.createElement('a');
		link.innerText = data.file || 'Download file';
		link.download = data.file || 'file.bin';
		link.href = data.link;
		const line = document.createElement('p');
		line.textContent = 'Download link: '
		line.appendChild(link);
		terminal.append(line);
		link.click();
		return;
	}
	if (data.list !== undefined) {
		fileList.innerHTML = '';
		for (let file of data.list) {
			const li = document.createElement('li');
			li.innerText = file;
			if (file.endsWith('/')) {
				li.classList.add('folder');
				li.onclick = function() {
					// update params to preserve history navigation
					params.update({folder: file});
				};
			} else {
				li.classList.add('file');
				li.onclick = function() {
					// update params to preserve history navigation
					params.update({file, line: null});
				};
				li.ondblclick = function() {
					openProjectFile({ file, link: true });
				};
			}
			if (file === params.file) {
				li.classList.add('active');
			}
			fileList.appendChild(li);
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
		spnStatus.innerText = null;
		params.update('workspace', 'project', 'content', 'folder', 'file');
	}
	if (data.progress !== undefined) {
		if (data.progress > 0) {
			spnStatus.innerText = 'Downloading files: ' + data.progress + ' / ' + data.total;
			return;
		}
		spnStatus.innerText = null;
		params.update('file', 'folder', 'content');
	}
}