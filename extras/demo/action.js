var customCommands = {
	'theme:': function(value) {
		if (value === undefined || value === '') {
			completeAction('!theme:');
			return false;
		}
		if (value === 'dark' || value === 'light') {
			params.update({theme: value});
			return true;
		}
		return actionError();
	},
	'zoom:': function(value) {
		if (value === undefined || value === '') {
			completeAction('!zoom:');
			return false;
		}
		document.body.style.fontSize = (+value / 100) + 'em';
		editor.refresh();
	},

	close: function(value) {
    	if (value !== undefined && value !== '') {
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

	selectLine: function () {
		let line = editor.getCursor().line || 0;
		editor.setSelection(
			CodeMirror.Pos(line, 0),
			CodeMirror.Pos(line + 1, 0)
		);
	},
	selectWord: function () {
		var sel = editor.findWordAt(editor.getCursor());
		editor.setSelection(sel.anchor, sel.head);
	},
};

function actionError() {
	edtFileName.classList.add('error');
	setTimeout(function() {
		edtFileName.classList.remove('error');
	}, 250);
	return false;
}

function completeAction(action, hint) {
	setStyle(document.body, 'tool-bar');
	edtFileName.focus();

	if (action == null) {
		return;
	}

    if (hint === true || hint === false) {
    	if (action.startsWith('!')) {
    		try {
				let command = action.substr(1);
				let filter = new RegExp(command, 'i');
				let actions = {};
				for (let cmd in customCommands) {
					if (cmd.search(filter) === -1) {
						continue;
					}
					actions['!' + cmd] = customCommands[cmd];
				}
				setActions(actions);
    		} catch(error) {
    			actionError();
    		}
		} else {
			setActions();
		}
		hint = hint ? undefined : '';
    }

	if (hint === undefined) {
		switch (action) {
			default:
				return;

			case '!zoom:':
				hint = '100';
				break;

			case '!theme:':
				hint = document.body.classList.contains('dark') ? 'light' : 'dark';
				break;

			case '!':
				hint = 'execute command';
				break;

			case '?':
				hint = editor.getSelection() || 'search text';
				break;

			case ':':
				hint = editor.getCursor().line + 1 || 'go to line number';
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
				completeAction(items[item], '');
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
		row.onclick = function() {
			let result = actions[action]();
			if (result === false) {
				return false;
			}
			setStyle(document.body, '-right-bar');
			edtFileName.blur();
		}
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
	let actions = {
		Command: function() {
			completeAction('!', true);
			return false;
		},
		Search: function() {
			completeAction('?', true);
			return false;
		},
		Goto: function() {
			completeAction(':', true);
			return false;
		},
		'Select All': customCommands.selectAll,
		'Select Line': customCommands.selectLine,
		'Select Word': customCommands.selectWord,
		'Insert Tab': function() {
			editor.execCommand('insertTab');
			return false;
		}
	};
	setActions(actions, function() {
		return false;
	});
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
	// `!command` => execute command
	if (action.startsWith('!')) {
		action = action.substr(1);
		for (let cmd in customCommands) {
			if (action.startsWith(cmd)) {
				if (action !== cmd && !cmd.endsWith(':')) {
					// not an executable command
					continue;
				}
				let value = action.substr(cmd.length);
				let result = customCommands[cmd](value);
				if (result === false) {
					continue;
				}
				setStyle(document.body, '-right-bar');
				edtFileName.blur();
				return false;
			}
		}
		return actionError();
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
edtFileName.oninput = function(event) {
	completeAction(edtFileName.value, event.inputType === 'insertText');
};

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
	if (customCommands.hasOwnProperty(command)) {
		// command is overwritten
		continue;
	}
	customCommands[command] = function() {
		return editor.execCommand(command);
	}
}
