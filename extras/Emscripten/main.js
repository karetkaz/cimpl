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
			cmdExecute.value = '-run/g';
			btnExecute.innerText = 'Run';
		} else {
			cmdExecute.value = params.exec;
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
		btnFileName.innerText = params.file;
	} else {
		document.title = docTitle;
		btnFileName.innerText = '';
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

editor.setSize('100%', '100%');

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
	let args = undefined;
	let file = undefined;

	if (text != null) {
		btnExecute.innerText = text;
	}
	if (cmd != null) {
		args = [];
		cmdExecute.value = cmd;

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

		for (let arg of cmd.split(' ')) {
			if (arg === '') {
				continue;
			}
			args.push(arg);
		}
	}

	showEditor('-primary');
	terminal.clear();
	execInput(args);
}
