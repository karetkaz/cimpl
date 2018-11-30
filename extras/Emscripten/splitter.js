function showSplitter(splitter, ...styles) {
	if (splitter && splitter.constructor === String) {
		// allow to reference the splitter using id.
		splitter = document.getElementById(splitter);
	}

	let cycleIndex = 0;
	let cycleItems = [];
	for (let style of styles) {

		// remove style
		if (style.startsWith('-')) {
			splitter.classList.remove(style.substring(1));
			continue;
		}

		// toggle style
		if (style.startsWith('!')) {
			style = style.substring(1);
			if (splitter.classList.contains(style)) {
				splitter.classList.remove(style);
				style = '';
			}
		}

		// cycle style
		if (style.startsWith('^')) {
			style = style.substring(1);
			cycleItems.push(style);
			if (splitter.classList.contains(style)) {
				splitter.classList.remove(style);
				cycleIndex = cycleItems.length;
			}
			style = '';
		}

		if (['vertical', 'horizontal', 'auto'].includes(style)) {
			splitter.classList.remove('horizontal');
			splitter.classList.remove('vertical');
			if (style === 'auto') {
				style = '';
			}
		}

		if (['primary', 'secondary', 'split'].includes(style)) {
			splitter.classList.remove('secondary');
			splitter.classList.remove('primary');
			if (style === 'split') {
				style = '';
			}
		}

		if (style !== '') {
			splitter.classList.add(style);
		}
	}
	if (cycleItems.length > 0) {
		let style = cycleItems[cycleIndex % cycleItems.length];
		if (style !== '') {
			splitter.classList.add(style);
		}
	}
}
