function actionError() {
	edtFileName.classList.add('error');
	setTimeout(function() {
		edtFileName.classList.remove('error');
	}, 250);
	return false;
}

function executeAction(action) {
	setStyle(document.body, '-right-bar');
	for (let command in customCommands) {
		if (action.startsWith(command)) {
			let value = action.substr(command.length);
			let result = customCommands[command](value, action);
			if (result === false) {
				continue;
			}
			if (result === true) {
				edtFileName.blur();
			}
			return false;
		}
	}

	if (CodeMirror.commands.hasOwnProperty(action)) {
		CodeMirror.commands[action](editor);
		edtFileName.blur();
		return false;
	}
	return actionError();
}
function completeAction(action, hint) {
	setStyle(document.body, 'tool-bar');
	edtFileName.focus();

	if (action == null) {
		return;
	}
	if (action.constructor === KeyboardEvent) {
		if (!edtFileName.value.endsWith(action.key)) {
			if (edtFileName.selectableItems != null) {
				switch (action.keyCode) {
					case 38: // up
						edtFileName.selectedItem -= 1;
						if (edtFileName.selectedItem < 0) {
							edtFileName.selectedItem = 0;
						}
						edtFileName.value = edtFileName.selectableItems[edtFileName.selectedItem];
						edtFileName.selectionStart = edtFileName.selectionEnd = edtFileName.value.length;
						return false;

					case 40: // down
						edtFileName.selectedItem += 1;
						if (edtFileName.selectedItem >= edtFileName.selectableItems.length) {
							edtFileName.selectedItem = edtFileName.selectableItems.length - 1;
						}
						edtFileName.value = edtFileName.selectableItems[edtFileName.selectedItem];
						edtFileName.selectionStart = edtFileName.selectionEnd = edtFileName.value.length;
						return false;
				}
			}
			// maybe backspace was pressed
			return;
		}
		action = edtFileName.value;
	}
	setStyle(document.body, '-right-bar');
	if (hint == null) {
		switch (action) {
			default:
				hint = '';
				break;

			case ':':
				hint = editor.getCursor().line + 1 || 'enter a line number to go to';
				break;

			case '!zoom:':
				hint = '100';
				break;

			case '?':
				hint = editor.getSelection() || 'enter a text to search';
				break;

			case '?*':
				hint = editor.getSelection() || 'enter a text to search and select occurrences';
				break;

			case '#':
				hint = terminal.command || '';
				break;

			case '!':
				let items = [];
				let commands = '';
				for (let cmd in customCommands) {
					commands += '<li onclick="completeAction(\'!'+cmd+'\');">' + cmd + '</li>';
					items.push('!' + cmd);
				}
				//*
				for (let cmd in CodeMirror.commands) {
					commands += '<li onclick="executeAction(\''+cmd+'\');">' + cmd + '</li>' 
					items.push('!' + cmd);
				}
				// */
				//edtFileName.selectableItems = items;
				if (isNaN(edtFileName.selectedItem)) {
					edtFileName.selectedItem = 0;
				}
				document.getElementById('right-sidebar').innerHTML = commands;
				setStyle(document.body, 'right-bar');
				hint = items[edtFileName.selectedItem].substr(1);
				break;
		}
	}

	edtFileName.value = action + hint;
	edtFileName.selectionStart = action.length;
	edtFileName.selectionEnd = edtFileName.value.length;
}

edtFileName.onfocus = function() {
	edtFileName.selectionStart = 0;
	edtFileName.selectionEnd = edtFileName.value.length;
}
edtFileName.onblur = function() {
	//setStyle(document.body, '-right-bar');
	edtFileName.value = params.file || '';
	edtFileName.selectableItems = null;
	//edtFileName.selectedItem = -1;
	editor.focus();
}
var customCommands = {
	'theme:': function(value, action) {
		if (value === 'dark') {
			setStyle(document.body, 'dark');
			return true;
		}
		if (value === 'light') {
			setStyle(document.body, '-dark');
			return true;
		}
		return actionError();
	},
	'zoom:': function(value, action) {
		document.body.style.fontSize = (+value / 100) + 'em';
		editor.refresh();
	},
	'close': function(value, action) {
		if (value !== '') {
			return false;
		}
		params.update({ content: null, file: null });
		return true;
	},
	'+': function(value, action) {
		if (value !== '') {
			return false;
		}
		editor.execCommand('unfoldAll');
		return true;
	},
	'-': function(value, action) {
		if (value !== '') {
			return false;
		}
		editor.execCommand('foldAll');
		return true;
	},
}

edtFileName.onkeydown = function(event) {
	if (event.key !== 'Enter') {
		return true;
	}

	let action = edtFileName.value;
	// `!commands` => execute command
	if (action.startsWith('!')) {
		return executeAction(action.substr(1));
	}

	// `#arguments` => execute script
	else if (action.startsWith('#')) {
		let command = action.substr(1);
		//params.update({exec: command});
		execute(command, true);
	}

	// `:<number>` => goto line
	else if (action.startsWith(':')) {
		let line = action.substr(1);
		if (isNaN(line)) {
			// todo: go to symbol in file
			actionError();
			return false;
		}
		params.update({line});
	}

	// ?* => find and select occurrences
	else if (action.startsWith('?*')) {
		let text = action.substr(2);
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
	else if (action.startsWith('?')) {
		let text = action.substr(1);
		let cursor = editor.getSearchCursor(text, 0);
		if (!cursor.findNext()) {
			edtFileName.cursor = null;
			return actionError();
		}
		editor.setSelection(cursor.from(), cursor.to());
		edtFileName.cursor = cursor;
	}

	// '[{...' => project
	else if (action.startsWith('[{')) {
		openProjectFile({
			project: JSON.parse(action),
			file: params.file,
			line: params.line
		});
	}

	/* close file
	else if (action === '') {
		params.update({ content: null, file: null });
	}*/

	// open or download file
	else {
		// [match, host, path, line, column, query, hash]
		let match = action.match(pathMather);
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

CodeMirror.commands.save = function(cm) {
	saveInput();
}
CodeMirror.commands.jumpToLine = function(cm) {
	let line = editor.getCursor().line || 0;
	completeAction(':', line + 1);
	var middleHeight = editor.getScrollerElement().offsetHeight / 2;
	var t = editor.charCoords({line, ch: 0}, "local").top;
	editor.scrollTo(null, t - middleHeight - 5);
	editor.setCursor(line);
}

CodeMirror.commands.find = function(cm) {
	completeAction('?');
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
