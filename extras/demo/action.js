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
			if (CodeMirror.commands.hasOwnProperty(action)) {
				CodeMirror.commands[action](editor);
				edtFileName.blur();
				return false;
			}
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

edtFileName.onfocus = function() {
	edtFileName.selectionStart = 0;
	edtFileName.selectionEnd = edtFileName.value.length;
}
edtFileName.onblur = function() {
	edtFileName.value = params.file || '';
	spnCounter.innerText = null;
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
	'download': function(value, action) {
		openProjectFile({
			file: params.file,
			link: true
		});
		return true;
	},
	save: CodeMirror.commands.save,
	selectAll: CodeMirror.commands.selectAll,
	// singleSelection: CodeMirror.commands.singleSelection,
	// killLine: CodeMirror.commands.killLine,
	deleteLine: CodeMirror.commands.deleteLine,
	// delLineLeft: CodeMirror.commands.delLineLeft,
	// delWrappedLineLeft: CodeMirror.commands.delWrappedLineLeft,
	// delWrappedLineRight: CodeMirror.commands.delWrappedLineRight,
	undo: CodeMirror.commands.undo,
	redo: CodeMirror.commands.redo,
	// undoSelection: CodeMirror.commands.undoSelection,
	// redoSelection: CodeMirror.commands.redoSelection,
	goDocStart: CodeMirror.commands.goDocStart,
	goDocEnd: CodeMirror.commands.goDocEnd,
	goLineStart: CodeMirror.commands.goLineStart,
	goLineStartSmart: CodeMirror.commands.goLineStartSmart,
	goLineEnd: CodeMirror.commands.goLineEnd,
	// goLineRight: CodeMirror.commands.goLineRight,
	// goLineLeft: CodeMirror.commands.goLineLeft,
	// goLineLeftSmart: CodeMirror.commands.goLineLeftSmart,
	// goLineUp: CodeMirror.commands.goLineUp,
	// goLineDown: CodeMirror.commands.goLineDown,
	// goPageUp: CodeMirror.commands.goPageUp,
	// goPageDown: CodeMirror.commands.goPageDown,
	// goCharLeft: CodeMirror.commands.goCharLeft,
	// goCharRight: CodeMirror.commands.goCharRight,
	// goColumnLeft: CodeMirror.commands.goColumnLeft,
	// goColumnRight: CodeMirror.commands.goColumnRight,
	goWordLeft: CodeMirror.commands.goWordLeft,
	goWordRight: CodeMirror.commands.goWordRight,
	// goGroupRight: CodeMirror.commands.goGroupRight,
	// goGroupLeft: CodeMirror.commands.goGroupLeft,
	// delCharBefore: CodeMirror.commands.delCharBefore,
	// delCharAfter: CodeMirror.commands.delCharAfter,
	// delWordBefore: CodeMirror.commands.delWordBefore,
	// delWordAfter: CodeMirror.commands.delWordAfter,
	// delGroupBefore: CodeMirror.commands.delGroupBefore,
	// delGroupAfter: CodeMirror.commands.delGroupAfter,
	indentAuto: CodeMirror.commands.indentAuto,
	indentMore: CodeMirror.commands.indentMore,
	indentLess: CodeMirror.commands.indentLess,
	insertTab: CodeMirror.commands.insertTab,
	// insertSoftTab: CodeMirror.commands.insertSoftTab,
	// defaultTab: CodeMirror.commands.defaultTab,
	// transposeChars: CodeMirror.commands.transposeChars,
	// newlineAndIndent: CodeMirror.commands.newlineAndIndent,
	// openLine: CodeMirror.commands.openLine,
	toggleOverwrite: CodeMirror.commands.toggleOverwrite,
	toggleComment: CodeMirror.commands.toggleComment,
	toggleCommentIndented: CodeMirror.commands.toggleCommentIndented,

	fold: CodeMirror.commands.fold,
	unfold: CodeMirror.commands.unfold,
	foldAll: CodeMirror.commands.foldAll,
	unfoldAll: CodeMirror.commands.unfoldAll,
	toggleFold: CodeMirror.commands.toggleFold,

	jumpToBrace: CodeMirror.commands.jumpToBrace,
	selectToBrace: CodeMirror.commands.selectToBrace,

	upperCase: CodeMirror.commands.upperCase,
	lowerCase: CodeMirror.commands.lowerCase,
	toggleCase: CodeMirror.commands.toggleCase,
	camelCase: CodeMirror.commands.camelCase,
	snakeCase: CodeMirror.commands.snakeCase,

	scrollLineUp: CodeMirror.commands.scrollLineUp,
	scrollLineDown: CodeMirror.commands.scrollLineDown,
	swapLineUp: CodeMirror.commands.swapLineUp,
	swapLineDown: CodeMirror.commands.swapLineDown,
	// save: CodeMirror.commands.save,
	// jumpToLine: CodeMirror.commands.jumpToLine,
	// find: CodeMirror.commands.find,
	// findNext: CodeMirror.commands.findNext,
	// findPrev: CodeMirror.commands.findPrev,
	// selectFound: CodeMirror.commands.selectFound
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
CodeMirror.commands.jumpToLine = function(cm) {
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
