// adapted from https://codemirror.net/keymap/sublime.js
CodeMirror.normalizeKeyMap(CodeMirror.keyMap.extraKeys = {
	"Ctrl--": "fold",	// todo: fold current block
	"Ctrl-=": "unfold",
	"Shift-Ctrl--": "foldAll",
	"Shift-Ctrl-=": "unfoldAll",

	"Ctrl-U": "toggleCase",
	"Shift-Ctrl-U": "toggleCase",
	// "Shift-Ctrl-J": "joinLines",
	// "Shift-Ctrl-S": "sortLines",
	"Ctrl-D": "duplicate",

	"Ctrl-B": "goToBrace",
	"Shift-Ctrl-B": "selectToBrace",

	"Shift-Tab": "indentLess",
	"Ctrl-/": "toggleCommentIndented",

	"Ctrl-Backspace": "delGroupBefore",
	"Ctrl-Delete": "delGroupAfter",
	"Shift-Delete": "deleteLine",

	"Ctrl-Home": "goDocStart",
	"Ctrl-End": "goDocEnd",

	"Shift-Ctrl-Up": "swapLineUp",
	"Shift-Ctrl-Down": "swapLineDown",

	"Ctrl-Up": "scrollLineUp",
	"Ctrl-Down": "scrollLineDown",
	"Ctrl-Left": "goGroupLeft",
	"Ctrl-Right": "goGroupRight",

	"Ctrl-A": "selectAll",
	"Ctrl-Z": "undo",
	"Shift-Ctrl-Z": "redo",

	"Ctrl-[": "indentLess",
	"Ctrl-]": "indentMore",

	"Ctrl-S": "save",
	"Ctrl-G": "goToLine",
	"Ctrl-F": "find",
	"F3": "findNext",
	"Shift-F3": "findPrev",
// 	"Ctrl-R": "replace",

// 	"Ctrl-U": "undoSelection",
// 	"Shift-Ctrl-F": "replace",
// 	"Shift-Ctrl-G": "findPrev",
// 	"Shift-Ctrl-R": "replaceAll",
// 	"Shift-Ctrl-U": "redoSelection",
// 	"Shift-Ctrl-Z": "redo",
	"fallthrough": "basic",
});

function modifySelection(cm, /*wordOrLine,*/ mod) {
	cm.operation(function () {
		let ranges = cm.listSelections(), replacements = [];
		for (let i = 0; i < ranges.length; i++) {
			let range = ranges[i];
			if (range.empty()) {
				// todo extend range to line or word
				replacements.push("");
			} else {
				replacements.push(mod(cm.getRange(range.from(), range.to())));
			}
		}
		cm.replaceSelections(replacements, "around", "case");
	});
}

CodeMirror.commands.goToBrace = function (cm) {
	let cursor = cm.getCursor();
	let result = cm.findMatchingBracket(cursor);
	if (result && result.match) {
		if (result.from.ch === cursor.ch) {
			result.to.ch += 1;
		}
		cm.setCursor(result.to);
	}
}
CodeMirror.commands.selectToBrace = function (cm) {
	let cursor = cm.getCursor();
	let result = cm.findMatchingBracket(cursor);
	if (result && result.match) {
		if (result.from.ch === cursor.ch) {
			result.to.ch += 1;
		} else {
			result.from.ch += 1;
		}
		cm.setSelection(result.from, result.to);
	}
}

CodeMirror.commands.toggleCommentIndented = function (cm) {
	cm.toggleComment({indent: true});
}

CodeMirror.commands.upperCase = function (cm) {
	modifySelection(cm, function (str) {
		return str.toUpperCase();
	});
}
CodeMirror.commands.lowerCase = function (cm) {
	modifySelection(cm, function (str) {
		return str.toLowerCase();
	});
}
CodeMirror.commands.toggleCase = function (cm) {
	modifySelection(cm, function (str) {
		let result = str.toLowerCase();
		if (str !== result) {
			return result;
		}
		return str.toUpperCase();
	});
}
CodeMirror.commands.camelCase = function (cm) {
	modifySelection(cm, function (str) {
		let result = '';
		for (let i = 0; i < str.length; ++i) {
			let ch = str.charAt(i);
			if (ch === ' ' || ch === '\t' || ch === '\v' || ch == '_') {
				// skip whitespaces, and '_'
				continue;
			}

			// not a number or lower or upper case character => convertUpperCase
			let chUpperCase = ch.toUpperCase();
			if (chUpperCase !== ch.toLowerCase()) {
				let prev = str.charAt(i - 1);
				let last = result.charAt(result.length - 1);
				let isLastNum = last >= '0' && last <= '9';
				if (prev !== last && (isLastNum || last !== last.toUpperCase())) {
					ch = chUpperCase;
				}
			}
			result += ch;
		}
		return result;
	});
}
CodeMirror.commands.snakeCase = function (cm) {
	modifySelection(cm, function (str) {
		let result = '';
		for (let i = 0; i < str.length; ++i) {
			let ch = str.charAt(i);
			if (ch === ' ' || ch === '\t' || ch === '\v') {
				// skip whitespaces
				continue;
			}

			// not a lower or upper case character => add_underscore
			let chUpperCase = ch.toUpperCase();
			if (chUpperCase !== ch.toLowerCase()) {
				let prev = str.charAt(i - 1);
				let last = result.charAt(result.length - 1);
				let isLastNum = last >= '0' && last <= '9';
				if (prev !== last && (isLastNum || last.toUpperCase() !== last.toLowerCase())) {
					// last lower or upper case character is folowed by white space
					result += '_';
				}
				else if (ch === chUpperCase && prev !== prev.toUpperCase()) {
					// convert from camelCase => camel_Case
					result += '_';
				}
			}
			result += ch;
		}
		return result;
	});
}

CodeMirror.commands.scrollLineUp = function (cm) {
	var info = cm.getScrollInfo();
	if (!cm.somethingSelected()) {
		var visibleBottomLine = cm.lineAtHeight(info.top + info.clientHeight, "local");
		if (cm.getCursor().line >= visibleBottomLine)
			cm.execCommand("goLineUp");
	}
	cm.scrollTo(null, info.top - cm.defaultTextHeight());
};
CodeMirror.commands.scrollLineDown = function (cm) {
	var info = cm.getScrollInfo();
	if (!cm.somethingSelected()) {
		var visibleTopLine = cm.lineAtHeight(info.top, "local") + 1;
		if (cm.getCursor().line <= visibleTopLine)
			cm.execCommand("goLineDown");
	}
	cm.scrollTo(null, info.top + cm.defaultTextHeight());
};

CodeMirror.commands.swapLineUp = function (cm) {
	if (cm.isReadOnly()) return CodeMirror.Pass
	var ranges = cm.listSelections(), linesToMove = [], at = cm.firstLine() - 1, newSels = [];
	const Pos = CodeMirror.Pos;
	for (var i = 0; i < ranges.length; i++) {
		var range = ranges[i], from = range.from().line - 1, to = range.to().line;
		newSels.push({
			anchor: Pos(range.anchor.line - 1, range.anchor.ch),
			head: Pos(range.head.line - 1, range.head.ch)
		});
		if (range.to().ch == 0 && !range.empty()) --to;
		if (from > at) linesToMove.push(from, to);
		else if (linesToMove.length) linesToMove[linesToMove.length - 1] = to;
		at = to;
	}
	cm.operation(function () {
		for (var i = 0; i < linesToMove.length; i += 2) {
			var from = linesToMove[i], to = linesToMove[i + 1];
			var line = cm.getLine(from);
			cm.replaceRange("", Pos(from, 0), Pos(from + 1, 0), "+swapLine");
			if (to > cm.lastLine())
				cm.replaceRange("\n" + line, Pos(cm.lastLine()), null, "+swapLine");
			else
				cm.replaceRange(line + "\n", Pos(to, 0), null, "+swapLine");
		}
		cm.setSelections(newSels);
		cm.scrollIntoView();
	});
};
CodeMirror.commands.swapLineDown = function (cm) {
	if (cm.isReadOnly()) return CodeMirror.Pass
	var ranges = cm.listSelections(), linesToMove = [], at = cm.lastLine() + 1;
	const Pos = CodeMirror.Pos;
	for (var i = ranges.length - 1; i >= 0; i--) {
		var range = ranges[i], from = range.to().line + 1, to = range.from().line;
		if (range.to().ch == 0 && !range.empty()) from--;
		if (from < at) linesToMove.push(from, to);
		else if (linesToMove.length) linesToMove[linesToMove.length - 1] = to;
		at = to;
	}
	cm.operation(function () {
		for (var i = linesToMove.length - 2; i >= 0; i -= 2) {
			var from = linesToMove[i], to = linesToMove[i + 1];
			var line = cm.getLine(from);
			if (from == cm.lastLine())
				cm.replaceRange("", Pos(from - 1), Pos(from), "+swapLine");
			else
				cm.replaceRange("", Pos(from, 0), Pos(from + 1, 0), "+swapLine");
			cm.replaceRange(line + "\n", Pos(to, 0), null, "+swapLine");
		}
		cm.scrollIntoView();
	});
};

CodeMirror.commands.duplicate = function(cm) {
	const Pos = CodeMirror.Pos;
	cm.operation(function() {
		var rangeCount = cm.listSelections().length;
		for (var i = 0; i < rangeCount; i++) {
			var range = cm.listSelections()[i];
			if (range.empty())
				cm.replaceRange(cm.getLine(range.head.line) + "\n", Pos(range.head.line, 0));
			else
				cm.replaceRange(cm.getRange(range.from(), range.to()), range.from());
		}
		cm.scrollIntoView();
	});
};
CodeMirror.commands.joinLines = function(cm) {
	const Pos = CodeMirror.Pos;
	var ranges = cm.listSelections(), joined = [];
	for (var i = 0; i < ranges.length; i++) {
		var range = ranges[i], from = range.from();
		var start = from.line, end = range.to().line;
		while (i < ranges.length - 1 && ranges[i + 1].from().line == end)
			end = ranges[++i].to().line;
		joined.push({start: start, end: end, anchor: !range.empty() && from});
	}
	cm.operation(function() {
		var offset = 0, ranges = [];
		for (var i = 0; i < joined.length; i++) {
			var obj = joined[i];
			var anchor = obj.anchor && Pos(obj.anchor.line - offset, obj.anchor.ch), head;
			for (var line = obj.start; line <= obj.end; line++) {
				var actual = line - offset;
				if (line == obj.end) head = Pos(actual, cm.getLine(actual).length + 1);
				if (actual < cm.lastLine()) {
					cm.replaceRange(" ", Pos(actual), Pos(actual + 1, /^\s*/.exec(cm.getLine(actual + 1))[0].length));
					++offset;
				}
			}
			ranges.push({anchor: anchor || head, head: head});
		}
		cm.setSelections(ranges, 0);
	});
};
CodeMirror.commands.sortLines = function(cm, ignoreCase) {
	const Pos = CodeMirror.Pos;
	if (cm.isReadOnly()) return CodeMirror.Pass
	var ranges = cm.listSelections(), toSort = [], selected;
	for (var i = 0; i < ranges.length; i++) {
		var range = ranges[i];
		if (range.empty()) continue;
		var from = range.from().line, to = range.to().line;
		while (i < ranges.length - 1 && ranges[i + 1].from().line == to)
			to = ranges[++i].to().line;
		if (!ranges[i].to().ch) to--;
		toSort.push(from, to);
	}
	if (toSort.length) selected = true;
	else toSort.push(cm.firstLine(), cm.lastLine());

	cm.operation(function() {
		var ranges = [];
		for (var i = 0; i < toSort.length; i += 2) {
			var from = toSort[i], to = toSort[i + 1];
			var start = Pos(from, 0), end = Pos(to);
			var lines = cm.getRange(start, end, false);
			if (ignoreCase === true)
				lines.sort();
			else
				lines.sort(function(a, b) {
					var au = a.toUpperCase(), bu = b.toUpperCase();
					if (au != bu) { a = au; b = bu; }
					return a < b ? -1 : a == b ? 0 : 1;
				});
			cm.replaceRange(lines, start, end);
			if (selected) ranges.push({anchor: start, head: Pos(to + 1, 0)});
		}
		if (selected) cm.setSelections(ranges, 0);
	});
}
CodeMirror.commands.sortLinesIgnoreCase = function(cm) {
	CodeMirror.commands.sortLines(cm, true);
};