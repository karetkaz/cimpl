importScripts("cmpl.js");

Module.print = function(text) {
	postMessage({ print: text });
};

Module.onRuntimeInitialized = function () {
	let filelist = [];
	FS.chdir('/workspace');
	let contents = FS.analyzePath(FS.cwd()).object.contents;
	for (let file in contents) {
		if (!contents.hasOwnProperty(file)) {
			continue;
		}
		if (contents[file].isFolder || contents[file].isDevice) {
			continue;
		}
		filelist.push(file);
	}
	postMessage({filelist: filelist});
};

onmessage = function(event) {
	let data = event.data;
	//postMessage({ "=>": JSON.stringify(data) });
	console.log(JSON.stringify(data));
	if (data.filename !== undefined) {
		if (data.content !== undefined) {
			FS.writeFile(data.filename, data.content);
		} else {
			try {
				let dat = FS.readFile(data.filename);
				let dec = new TextDecoder('utf-8');
				postMessage({
					content: dec.decode(dat.buffer)
				});
			} catch (err) {
				postMessage({
					print: 'opening file failed: `' + data.filename + '`: ' + err
				});
			}
		}
	}
	if (data.execute !== undefined) {
		Module['callMain'](data.execute);
		postMessage({ execute: 'success' });
	}
}