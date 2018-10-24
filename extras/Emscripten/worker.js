/* communication with worker/args
send => {
	files - download these files to the workspace directory
	file - open or save the content of the file
	line - jump to / position in file
	data(content) - save the content
	exec - arguments for the compiler
}

receive => {
	files - list of files in the workspace
	data(content) - content of the requested file
	line - jump to / position in file
	print - print the message to the output
}

args => {
	files - download files to a new workspace
	file - the file to be opened
	data(content) - overwrite the content of the file
	exec
	theme
	dump
	log
	mem
	X
}*/

var Module = {
	workspace: '/workspace',
	files: null,
	initialized: false,
	print: function(text) {
		postMessage({ print: text });
	},
	workspaceFiles: function() {
		let result = [];
		function lsr(path) {
			let dir = FS.analyzePath(Module.workspace + '/' + path);
			if (dir && dir.exists && dir.object) {
				for (let file in dir.object.contents) {
					if (dir.object.contents[file].isFolder) {
						lsr((path ? path + '/' : '') + file);
					} else {
						result.push((path ? path + '/' : '') + file);
					}
				}
			}
		}
		lsr('');
		return result;
	},
	onRuntimeInitialized: function () {
		FS.mkdirTree(Module.workspace);
		FS.chdir(Module.workspace);
		if (Module.files === null || Module.initialized) {
			return;
		}

		for (let file of Module.files) {
			let path = Module.workspace + '/' + file.path;
			try {
				if (file.url === undefined) {
					FS.mkdirTree(path.replace(/^(.*[/])?(.*)(\..*)$/, "$1"));
					FS.writeFile(path, file.content, {encoding: 'utf8'});
					Module.print('file created: ' + file.path);
					continue;
				}

				let xhr = new XMLHttpRequest();
				xhr.open('GET', file.url, false);
				xhr.send(null);
				if (!xhr.response || xhr.status < 200 || xhr.status >= 300) {
					throw new Error(xhr.status + ' (' + xhr.statusText + ')');
				}
				FS.mkdirTree(path.replace(/^(.*[/])?(.*)(\..*)$/, "$1"));
				FS.writeFile(path, xhr.responseText, {encoding: 'utf8'});
				Module.print('file downloaded: ' + xhr.responseURL);
			}
			catch (err) {
				Module.print('failed to download: `' + file.path + '`: ' + err);
				console.error(err);
			}
		}
		postMessage({files: Module.workspaceFiles()});
		Module.initialized = true;
	}
};

importScripts("cmpl.js");

onmessage = function(event) {
	let data = event.data;
	//console.log(data);
	if (data.files !== undefined) {
		Module.files = data.files;
		Module.onRuntimeInitialized();
	}
	if (data.file !== undefined) {
		let path = data.file;
		if (!path.startsWith('/')) {
			path = Module.workspace + '/' + path;
		}
		try {
			if (data.content !== undefined) {
				let exists = FS.analyzePath(path).exists;
				FS.mkdirTree(path.replace(/^(.*[/])?(.*)(\..*)$/, "$1"));
				FS.writeFile(path, data.content, {encoding: 'utf8'});
				postMessage({
					files: exists ? undefined : Module.workspaceFiles(),
					file: data.file,
					line: data.line
				});
			} else {
				postMessage({
					content: FS.readFile(path, {encoding: 'utf8'}),
					file: data.file,
					line: data.line
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