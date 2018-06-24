function Terminal(output, interpret) {
	if (output == null) {
		return {
			print: console.log,
			clear: console.clear
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

		for (let text of buffer) {
			let row = document.createElement("p");
			row.innerHTML = text;
			output.appendChild(row);
		}

		output.scrollTop = output.scrollHeight;
		buffer = [];
	}
	return {
		print: function (text) {
			let escaped = text.replace(/[<>&\r\n]/g, function (match) {
				switch (match) {
					default: return match;
					case '<': return "&lt;";
					case '>': return "&gt;";
					case '&': return "&amp;";
					case '\r': return "<br>";
					case '\n': return "<br>";
				}
			});
			buffer.push(interpret(escaped, text));
			if (nextFlush === 0) {
				nextFlush = setInterval(flush, 1000 / 20);	// @20 fps
			}
			//if (buffer.length > 1024) { flush(); }
		},
		clear: function () {
			output.innerHTML = '';
			buffer = [];
		}
	};
}
