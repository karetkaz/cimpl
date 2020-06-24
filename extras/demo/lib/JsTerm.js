function Terminal(output, interpret) {
	let scrollToEnd = true;
	if (output == null) {
		return {
			scroll: function(){},
			append: console.log,
			clear: console.clear,
			text: function () {
				return '';
			}
		}
	}
	if (interpret == null) {
		interpret = function(text) {
			return text;
		}
	}

	let nextFlush = 0;
	let buffer = [];
	function flush() {
		// nothing to print
		if (buffer.length == 0) {
			if (nextFlush !== 0) {
				clearInterval(nextFlush);
				nextFlush = 0;
			}
			return;
		}

		for (let line of buffer) {
			if (line instanceof HTMLElement) {
				output.appendChild(line);
				continue;
			}
			let row = document.createElement("p");
			row.innerHTML = line;
			output.appendChild(row);
		}

		if (scrollToEnd === true) {
			output.scrollTop = output.scrollHeight;
		}
		buffer = [];
	}
	output.onscroll = function(event) {
		if (scrollToEnd === true) {
			scrollToEnd = output.scrollTop === output.scrollHeight - output.clientHeight;
		}
	}
	return {
		scroll: function(end) {
			scrollToEnd = end;
			if (end) {
				output.scrollTop = output.scrollHeight;
			} else {
				output.scrollTop = 0;
			}
		},
		append: function(line) {
			if (line instanceof HTMLElement) {
				buffer.push(line);
			} else {
				let escaped = line.replace(/[<>&\r\n]/g, function (match) {
					switch (match) {
						default: return match;
						case '<': return "&lt;";
						case '>': return "&gt;";
						case '&': return "&amp;";
						case '\r': return "<br>";
						case '\n': return "<br>";
					}
				});
				buffer.push(interpret(escaped, line));
			}
			if (nextFlush === 0) {
				nextFlush = setInterval(flush, 1000 / 20);	// @20 fps
			}
			//if (buffer.length > 1024) { flush(); }
		},
		clear: function () {
			output.innerHTML = '';
			scrollToEnd = true;
			buffer = [];
		},
		text: function() {
			let result = '';
			for (let node of output.childNodes) {
				result += node.textContent;
				result += '\n';
			}
			return result;
		}
	};
}
