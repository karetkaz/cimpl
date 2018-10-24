let docTitle = document.title;
let worker = new Worker('worker.js');
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

	// setup theme
	if (!changes || changes.theme != null) {
		editor.setOption("theme", params.theme || "default");
	}

	// setup project
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
		worker.postMessage({files});
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
			worker.postMessage({
				file: params.file,
				line: params.line,
			});
		} else {
			editor.setValue('');
		}
	}

	if (changes.line && params.line != null) {
		editor.setCursor(params.line - 1);
	}
});

editor.setSize('100%', '100%');
worker.onmessage = function(event) {
	let data = event.data;
	//console.log('worker: ', data);
	if (data.print !== undefined) {
		terminal.print(data.print);
	}
	if (data.content !== undefined) {
		editor.setValue(data.content);
	}
	if (data.file !== undefined) {
		btnFileName.innerText = data.file;
		params.update({file: data.file, content: null});
	}
	if (data.line !== undefined) {
		params.update({line: data.line});
	}
	if (data.files !== undefined) {
		fileList.innerHTML = '';
		for (let file of data.files) {
			fileList.innerHTML += '<li onclick="showFile(this.innerText);">' + file + '</li>';
		}
		params.update('file', 'content');
	}
};

function showSplit(splitter, orientation) {
	splitter.classList.remove('primary');
	splitter.classList.remove('secondary');
	splitter.classList.remove('vertical');
	splitter.classList.remove('horizontal');

	if (orientation === false) {
		splitter.classList.add('primary');
	}
	else if (orientation === true) {
		splitter.classList.add('secondary');
	}
	if (orientation === 'vertical') {
		splitter.classList.add('vertical');
	}
	else if (orientation === 'horizontal') {
		splitter.classList.add('horizontal');
	}
	editor.setSize('100%', '100%');
}
function showMenu(menu) {
	if (menu && menu.style.display !== 'none') {
		// menu is opened, hide it
		menu = null;
	}
	sidebar.style.display = 'none';
	options.style.display = 'none';
	files.style.display = 'none';

	if (menu != null) {
		sidebar.style.display = 'block';
		menu.style.display = 'block';
	}
}
function showFile(file, line, column) {
	editor.setCursor((line || 1) - 1, column);
	if (file == null || file != params.file) {
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
					path: file.replace(/^(.*[/])?(.*)(\..*)$/, "$2$3"),
					url: file
				});
			}
		}
		content = '';
		for (let file of files) {
			if (content != '') {
				content += ", ";
			}
			content += JSON.stringify(file, null, '\t');
		}
		content = '[' + content + ']';
		params.update({content: btoa(content), project: undefined, file: undefined, line: undefined});
	}
}
function shareInput() {
	let content = editor.getValue();
	if (content.startsWith('[{')) {
		terminal.print('project file: ' + window.location.origin + window.location.pathname + '#project=' + btoa(content));
	}
	terminal.print('current file: ' + window.location.origin + window.location.pathname + '#content=' + btoa(content));
	terminal.print('decoded uri: ' + decodeURIComponent(window.location));

}
function saveInput() {
	let file = params.file;
	if (file == null) {
		file = prompt('Save file as:', 'untitled.ci');
	}
	if (file == null) {
		return;
	}
	worker.postMessage({
		content: editor.getValue(),
		file: file
	});
}

function execute(text, cmd) {
	let args = [];
	let file = undefined;

	if (text != null) {
		btnExecute.innerText = text;
	}
	if (cmd != null) {
		cmdExecute.value = cmd;


		// standard library is in root
		args.push('-std/stdlib.ci');

		// allocate 2Mb of memory by default,
		args.push('-mem' + (params.mem || '2M'));

		// do not use standard input, print times
		args.push('-X' + (params.X || '-stdin+times'));

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

		// compile the editing file
		file = params.file || 'new.ci';
		args.push(file);
	}

	terminal.clear();
	worker.postMessage({
		content: editor.getValue(),
		file: file,
		exec: args
	});
}
