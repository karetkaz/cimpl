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

	save: CodeMirror.commands.save,
	close: function(value, action) {
		if (value !== '') {
			return false;
		}
		params.update({ content: null, file: null });
		return true;
	},
	download: function(value, action) {
		openProjectFile({
			file: params.file,
			link: true
		});
		return true;
	},

	'!selectAll': CodeMirror.commands.selectAll,
	'!selectLine': function () {
		let line = editor.getCursor().line || 0;
		editor.setSelection(
			CodeMirror.Pos(line, 0),
			CodeMirror.Pos(line + 1, 0)
		);
	},
	'!selectWord': function () {
		var sel = editor.findWordAt(editor.getCursor());
		editor.setSelection(sel.anchor, sel.head);
	},
	'!selectToBrace': CodeMirror.commands.selectToBrace,

	'!goLineStartSmart': CodeMirror.commands.goLineStartSmart,
	'!goLineStart': CodeMirror.commands.goLineStart,
	'!goLineEnd': CodeMirror.commands.goLineEnd,
	'!goDocStart': CodeMirror.commands.goDocStart,
	'!goDocEnd': CodeMirror.commands.goDocEnd,

	'!insertTab': CodeMirror.commands.insertTab,
};

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
		let cmd = command;
		if (cmd.startsWith('!')) {
			cmd = cmd.substr(1);
		}
		if (action.startsWith(cmd)) {
			if (CodeMirror.commands.hasOwnProperty(action)) {
				CodeMirror.commands[action](editor);
				edtFileName.blur();
				return false;
			}
			let value = action.substr(cmd.length);
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
	return actionError();
}

function completeAction(action, hint) {
	setStyle(document.body, 'tool-bar');
	edtFileName.focus();

	if (action == null) {
		return;
	}
	if (action.constructor === KeyboardEvent) {
		if (edtFileName.value === '' || action.key === 'Backspace' || action.key === 'Delete') {
			setStyle(document.body, '-right-bar');
		}
		// todo: find a better way to detect if user typed or selected
		if (edtFileName.value.endsWith(action.key) || action.key === 'Backspace' || action.key === 'Delete') {
			if (edtFileName.value.startsWith('!')) {
				let command = edtFileName.value.substr(1);
				let filter = new RegExp(command, 'i');
				let actions = {};
				for (let cmd in customCommands) {
					if (cmd.startsWith('!')) {
						cmd = cmd.substr(1);
					}
					if (cmd.search(filter) === -1) {
						continue;
					}
					if (cmd.endsWith(':')) {
						actions['!' + cmd] = function() { completeAction('!' + cmd); }
					} else {
						actions['!' + cmd] = function() { executeAction(cmd); }
					}
				}
				setActions(actions);
			}
			if (action.key === 'Backspace' || action.key === 'Delete') {
				// do not show the hint if user deletes text
				return;
			}
		} else {
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
				hint = editor.getCursor().line + 1 || 'go to line number';
				setActions();
				break;

			case '!zoom:':
				hint = '100';
				break;

			case '!theme:':
				hint = document.body.classList.contains('dark') ? 'light' : 'dark';
				break;

			case '?':
				hint = editor.getSelection() || 'search text';
				setActions();
				break;

			case '#':
				hint = terminal.command || '';
				setActions();
				break;

			case '!':
				hint = 'execute command';
				break;
		}
	}

	edtFileName.value = action + hint;
	edtFileName.selectionStart = action.length;
	edtFileName.selectionEnd = edtFileName.value.length;
}

function setActions(actions, nextPrev) {
	//let commands = document.getElementById('commands');
	commands.innerHTML = null;
	spnCounter.innerText = null;
	if (actions == null) {
		setStyle(document.body, '-right-bar');
		commands.onNextPrev = undefined;
		return;
	}

	if (nextPrev === undefined) {
		let item = -1;
		let items = Object.keys(actions);
		nextPrev = function(direction) {
			if (direction == null || direction.constructor != Number) {
				return setActions();
			}

			if (document.activeElement != edtFileName) {
				return actionError();
			}
			item += direction;
			let error = false;
			if (item >= items.length && direction !== 0) {
				item = items.length - 1;
				error = true;
			}
			if (item < 0 && direction !== 0) {
				item = 0;
				error = true;
			}
			spnCounter.innerText = '' + (item + 1) + ' / ' + items.length;
			if (direction !== 0) {
				completeAction(items[item]);
				if (error) {
					actionError();
				}
			}
			return false;
		}
	}

	commands.onNextPrev = nextPrev;

	for (let action in actions) {
		let row = document.createElement('li');
		row.innerText = action;
		row.onclick = actions[action];
		commands.appendChild(row);
	}
	if (commands.childElementCount > 0) {
		// show sidebar
		setStyle(document.body, 'right-bar');
	}
	// show and select first item
	nextPrev(0);
}

edtFileName.onclick = function() {
	if (!props.mobile) {
		return;
	}
	let actions = {};
	for (let cmd in customCommands) {
		if (!cmd.startsWith('!')) {
			// hidden command
			continue;
			// cmd = '!' + cmd;
		}
		if (cmd.endsWith(':')) {
			actions[cmd] = function () {
				completeAction(cmd);
			}
		} else {
			actions[cmd] = function () {
				executeAction(cmd.substr(1));
			}
		}
	}
	setActions(actions);
}
edtFileName.onfocus = function() {
	edtFileName.selectionStart = 0;
	edtFileName.selectionEnd = edtFileName.value.length;
}
edtFileName.onblur = function() {
	edtFileName.value = params.file || '';
	spnCounter.innerText = null;
	editor.focus();
}

edtFileName.onkeydown = function(event) {
	if (event.key !== 'Enter') {
		if (commands.onNextPrev != null) {
			switch (event.keyCode) {
				case 38: // up
					return commands.onNextPrev(-1);

				case 40: // down
					return commands.onNextPrev(+1);
			}
		}
		return true;
	}

	let action = edtFileName.value;
	// `!commands` => execute command
	if (action.startsWith('!')) {
		let command = action.substr(1);
		return executeAction(command);
	}

	// `#arguments` => execute script
	else if (action.startsWith('#')) {
		let command = action.substr(1);
		//params.update({exec: command});
		execute(command, false);
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

	// ? => find text
	else if (action.startsWith('?')) {
		let text = action.substr(1);
		let cursor = editor.getSearchCursor(text, 0);
		let selection = 0;
		let selections = [];
		while (cursor.findNext()) {
			let from = cursor.from();
			let to = cursor.to();
			selections.push({
				anchor: from,
				head: to
			});
		}
		if (selections.length === 0) {
			setActions();
			return actionError();
		}
		editor.setCursor(selections[0].head);
		if (event.altKey) {
			editor.setSelections(selections);
			editor.focus();
			return false;
		}
		setActions({}, function(direction) {
			if (direction === 'all') {
				editor.setCursor(selections[0].head);
				editor.setSelections(selections);
				editor.focus();
				return setActions();
			}
			if (direction == null || direction.constructor != Number) {
				return setActions();
			}
			selection += direction;
			if (selection >= selections.length) {
				selection = selections.length - 1;
				return actionError();
			}
			if (selection < 0) {
				selection = 0;
				return actionError();
			}
			let pos = selections[selection];
			editor.setSelection(pos.anchor, pos.head);
			if (document.activeElement === edtFileName) {
				edtFileName.value = action;
				spnCounter.innerText = '' + (selection + 1) + ' / ' + selections.length;
			}
		});
		return false;
	}

	// '[{...' => project
	else if (action.startsWith('[{')) {
		openProjectFile({
			project: JSON.parse(action),
			file: params.file,
			line: params.line
		});
	}
	else if (action === '-') {
		editor.execCommand('foldAll');
	}
	else if (action === '+') {
		editor.execCommand('unfoldAll');
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
			file = file.replace(/^(.*[/])?(.*)(\..*)$/, '$2$3');
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
CodeMirror.commands.goToLine = function(cm) {
	let line = editor.getCursor().line || 0;
	completeAction(':', line + 1);
	var middleHeight = editor.getScrollerElement().offsetHeight / 2;
	var t = editor.charCoords({line, ch: 0}, 'local').top;
	editor.scrollTo(null, t - middleHeight - 5);
	editor.setCursor(line);
}

CodeMirror.commands.find = function(cm) {
	completeAction('?');
}
CodeMirror.commands.findNext = function(cm) {
	if (commands.onNextPrev == null) {
		return actionError();
	}
	commands.onNextPrev(+1);
}
CodeMirror.commands.findPrev = function(cm) {
	if (commands.onNextPrev == null) {
		return actionError();
	}
	commands.onNextPrev(-1);
}
CodeMirror.commands.selectFound = function(cm) {
	if (commands.onNextPrev == null) {
		return actionError();
	}
	commands.onNextPrev('all');
}

for (let command in CodeMirror.commands) {
	if (command.startsWith('!')) {
		command = command.substr(1);
	}
	if (customCommands.hasOwnProperty(command)) {
		continue;
	}
	customCommands[command] = CodeMirror.commands[command];
}
