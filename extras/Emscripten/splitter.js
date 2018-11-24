function showSpliter(splitter, primary, ...showElements) {
	if (splitter && splitter.constructor === String) {
		// allow to reference the splitter using id.
		splitter = document.getElementById(splitter);
	}

	if (primary.startsWith('~')) {
		splitter.classList.remove(primary.substring(1));
		return;
	}

	let addClass = true;
	if (primary.startsWith('!')) {
		primary = primary.substring(1);
		addClass = !splitter.classList.contains(primary);
	}
	if (['vertical', 'horizontal', 'auto'].includes(primary)) {
		splitter.classList.remove('horizontal');
		splitter.classList.remove('vertical');
		if (addClass && primary !== 'auto') {
			splitter.classList.add(primary);
		}
		return;
	}

	for (let elementId of showElements) {
		let visible = document.getElementById(elementId);
		for (let element of visible.parentElement.children) {
			if (element === visible) {
				if (element.style.display === 'none') {
					element.style.display = null;
					addClass = false;
				}
			} else {
				element.style.display = 'none';
			}
		}
	}

	if (['primary', 'secondary', 'split'].includes(primary)) {
		splitter.classList.remove('secondary');
		splitter.classList.remove('primary');
		if (addClass && primary !== 'split') {
			splitter.classList.add(primary);
		}
	}
}
