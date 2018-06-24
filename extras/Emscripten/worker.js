var Module = {
	workspace: '/workspace',
	files: [],
	print: function(text) {
		postMessage({ print: text });
	},
	onRuntimeInitialized: function () {
		FS.mkdirTree(Module.workspace);
		FS.chdir(Module.workspace);
		let files = [];
		for (let path of Module.files) {
			try {
				let xhr = new XMLHttpRequest();
				xhr.open('GET', path, false);
				xhr.send(null);
				if (!xhr.response || xhr.status < 200 || xhr.status >= 300) {
					throw new Error(xhr.status + ' (' + xhr.statusText + ')');
				}
				let file = path.replace(/^(.*[/])?(.*)(\..*)$/, "$2$3");
				FS.writeFile(file, xhr.responseText, {encoding: 'utf8'});
				files.push(file);
			}
			catch (err) {
				Module.print('file download failed: `' + path + '`: ' + err);
			}
		}
		postMessage({files: files});
	}
};

importScripts("cmpl.js");

onmessage = function(event) {
	let data = event.data;
	if (data.files !== undefined) {
		Module.files = data.files;
	}
	if (data.file !== undefined) {
		try {
			if (data.content !== undefined) {
				FS.writeFile(data.file, data.content, {encoding: 'utf8'});
				postMessage({
					file: data.file,
					line: data.line
				});
			} else {
				postMessage({
					file: data.file,
					line: data.line,
					content: FS.readFile(data.file, {encoding: 'utf8'})
				});
			}
		} catch (err) {
			Module.print('file operation failed: `' + data.file + '`: ' + err)
		}
	}
	if (data.exec !== undefined) {
		Module['callMain'](data.exec);
		postMessage({ exec: 'success' });
	}
};