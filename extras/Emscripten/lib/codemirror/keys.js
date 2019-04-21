CodeMirror.normalizeKeyMap(CodeMirror.keyMap.extraKeys = {
	"Ctrl--": "fold",	// todo: fold current block
	"Ctrl-=": "unfold",
	"Shift-Ctrl--": "foldAll",
	"Shift-Ctrl-=": "unfoldAll",

	"Ctrl-U": "toggleCase",
	"Shift-Ctrl-U": "toggleCase",
	//"Shift-Ctrl-J": "joinLines",
	//"Shift-Ctrl-S": "sortLines",
	//"Ctrl-Enter": "splitLine",

	//"Ctrl-E": "jumpToMatchBrace",
	//"Shift-Ctrl-E": "selectToMatchBrace",

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
	"Ctrl-G": "line",
	"Ctrl-F": "find",
	"Ctrl-R": "replace",
	"F3": "findNext",
	"Shift-F3": "findPrev",

// 	"Ctrl-U": "undoSelection",
// 	"Shift-Ctrl-F": "replace",
// 	"Shift-Ctrl-G": "findPrev",
// 	"Shift-Ctrl-R": "replaceAll",
// 	"Shift-Ctrl-U": "redoSelection",
// 	"Shift-Ctrl-Z": "redo",
	"fallthrough": "basic",
});

function modifyWordOrSelection(cm, mod) {
	cm.operation(function () {
		var ranges = cm.listSelections(), indices = [], replacements = [];
		for (var i = 0; i < ranges.length; i++) {
			var range = ranges[i];
			if (range.empty()) {
				indices.push(i);
				replacements.push("");
			} else replacements.push(mod(cm.getRange(range.from(), range.to())));
		}
		cm.replaceSelections(replacements, "around", "case");
		for (var i = indices.length - 1, at; i >= 0; i--) {
			var range = ranges[indices[i]];
			if (at && CodeMirror.cmpPos(range.head, at) > 0) continue;
			var word = wordAt(cm, range.head);
			at = word.from;
			cm.replaceRange(mod(word.word), word.from, word.to);
		}
	});
}

CodeMirror.commands.toggleCommentIndented = function (cm) {
	cm.toggleComment({indent: true});
}

CodeMirror.commands.toggleCase = function (cm) {
	modifyWordOrSelection(cm, function (str) {
		let result = str.toLowerCase();
		if (str !== result) {
			return result;
		}
		return str.toUpperCase();
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
