var Module = {
	initialized: false,
	relativeUrl: function(path) {
		let worker = '/extras/demo/emscripten/worker.js';
		if (location.href.endsWith(worker) && !path.startsWith('/')) {
			return location.href.substr(0, location.href.length - worker.length + 1) + path;
		}
		return path;
	},
	locateFile: function(path) {
		return './' + path;
	},
	dynamicLibraries: [
		'libFile.wasm'
	],
	print: function(message) {
		// console.log(message);
		postMessage({ print: message });
	},
	printErr: function(message) {
		console.error(message);
		postMessage({ error: message });
	},
	onRuntimeInitialized: function () {
		ENV.CMPL_HOME = '/';
		FS.mkdirTree(Module.workspace);
		FS.chdir(Module.workspace);
		Module.initialized = true;
		postMessage({initialized: true});
	},
	onFileDownloaded: function(progress, total) {
		postMessage({progress, total});
	}
};

importScripts("module.js");
importScripts("cmpl.js");

/* communication with worker/args
request => {
	list - download these files to the workspace directory [{path|file, url|content}]
	file - open or save the content of the file
	line - jump to / position in file
	url - download and save content
	content - save content
	workspace - switch workspace
	execute - arguments for the compiler
}

response => {
	list - list of files in the workspace
	data - content of the requested file
	line - jump to / position in file
	print - print the message to the output
	error - print the error to the output
	exitcode - response to execute
	initialized
}

args => {
	project - download the content of the given file list
	content - overwrite the content of the editor
	file - name of file to be opened
	line - line position to jump to
	theme - override default: &theme=light|dark
	show - override default: &show=editor|output

	exec - override default: &exec=-profile/G/H/P/T
	dump - dump output to file: &dump=dump.txt|dump.json
	log - log output to file: &log=dump.txt
	mem - if 2MB memory is not enough: &mem=128M
	X
}*/
onmessage = function(event) {
	let data = event.data;
	//console.log(data);
	try {
		Module.openProjectFile(data, function(result) {
			// execute
			if (data.execute !== undefined) {
				try {
					callMain(data.execute);
					result.exitCode = 0;
					sync = true;
				} catch (error) {
					result.exitCode = -1;
				}
			}
			postMessage(result);
		});
	} catch (err) {
		Module.printErr('operation failed: ' + err);
	}
};
