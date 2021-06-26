let projects = {
	'cmplStd/tests' : {
		base: 'cmplStd/test/',
		libs: ['libFile'],
		main: 'cmplStd/test/test.ci',
		files: [
			'lang/recPacking.ci',
			'lang/inlineMacros.ci',
			'lang/init.reference.ci',
			'lang/init.array.ci',
			'lang/stmt.for.ci',
			'lang/init.method.ci',
			'lang/overload.inline.ci',
			'lang/useOperator.ci',
			'lang/reflect.ci',
			'std/test.math.ci',
			'std/number.ci',
			'std/test.trigonometry.ci',
			'std/math.Complex.ci',
			'lang/emit.ci',
			'test.ci',
			'lang/init.member.ci',
			'std/tryExec.ci',
			'std/math.Bits.ci',
			'std/memory.ci',
			'lang/function.ci',
			'lang/stmt.if.ci',
			'lang/init.variable.ci',
			'lang/recUnion.ci',
			'demo/BitwiseArithmetic.ci',
		]
	},
	'cmplGfx/tests' : {
		base: 'cmplGfx/test/',
		libs: ['libGfx', 'libFile'],
		main: 'cmplGfx/test/testGfx.ci',
		files: [
			'cmplGfx/test/asset/font/NokiaPureText.ttf',
			'cmplGfx/test/asset/font/NokiaPureTextBold.ttf',
			'cmplGfx/test/asset/font/NokiaPureTextLight.ttf',
			'cmplGfx/test/asset/image/david.png',
			'cmplGfx/test/asset/image/forest.jpg',
			'cmplGfx/test/asset/image/garden.png',
			'cmplGfx/test/asset/image/heightmap.png',
			'cmplGfx/test/asset/image/texture.png',
			'cmplGfx/test/asset/image/texture_nature_01.png',
			'cmplGfx/test/asset/mesh/teapot.obj',
			'cmplGfx/test/demo/2d.draw.bezier.ci',
			'cmplGfx/test/demo/2d.L-System.ci',
			'cmplGfx/test/demo/AnalogClock.ci',
			'cmplGfx/test/demo/FloodIt.ci',
			'cmplGfx/test/demo/RayTracer0.ci',
			'cmplGfx/test/demo/StarField.ci',
			'cmplGfx/test/demo.procedural/Blobs.ci',
			'cmplGfx/test/demo.procedural/Complex.ci',
			'cmplGfx/test/demo.procedural/Mandelbrot.ci',
			'cmplGfx/test/demo.procedural/YinYang.ci',
			'cmplGfx/test/demo.widget/Calculator0.ci',
			'cmplGfx/test/demo.widget/DatePicker.ci',
			'cmplGfx/test/demo.widget/layout.Align.ci',
			'cmplGfx/test/demo.widget/layout.Padding.ci',
			'cmplGfx/test/2d.draw.image.ci',
			'cmplGfx/test/2d.ImageDiff.ci',
			'cmplGfx/test/3d.draw.mesh.ci',
			'cmplGfx/test/3d.height.map.ci',
			'cmplGfx/test/3d.parametric.ci',
			'cmplGfx/test/Calculator.ci',
			'cmplGfx/test/ColorPicker.ci',
			'cmplGfx/test/fx.color.blend.ci',
			'cmplGfx/test/fx.color.lookup.ci',
			'cmplGfx/test/fx.color.lookup-ease.ci',
			'cmplGfx/test/fx.color.lookup-levels.ci',
			'cmplGfx/test/fx.color.lookup-manual.ci',
			'cmplGfx/test/fx.color.matrix.ci',
			'cmplGfx/test/fx.color.select-hsl.ci',
			'cmplGfx/test/fx.histogram.ci',
			'cmplGfx/test/fx.image.blur.ci',
			'cmplGfx/test/fx.image.transform.ci',
			'cmplGfx/test/RayTracer.ci',
			'cmplGfx/test/testGfx.ci',
		]
	},
	'initialization' : {
		base: 'cmplStd/test/lang/',
		main: 'function.ci',
		files: [
			'function.ci',
			'init.array.ci',
			'init.member.ci',
			'init.method.ci',
			'init.variable.ci',
		]
	},
	'image edit' : {
		base: 'cmplGfx/test/',
		libs: ['libGfx', 'libFile'],
		files: [
			'asset/image/forest.jpg',
			'asset/image/texture.png',
			'asset/image/texture_nature_01.png',
			'fx.color.blend.ci',
			'fx.color.lookup-levels.ci',
			'fx.color.lookup-manual.ci',
			'fx.color.lookup.ci',
			'fx.color.matrix.ci',
			'fx.color.select-hsl.ci',
			'fx.histogram.ci',
			'fx.image.blur.ci',
			'fx.image.transform.ci',
		]
	},
};

(function () {
	// add scripts saved in indexed database
	if (indexedDB.databases != null) {
		workspaceName.innerText = 's';
		indexedDB.databases().then(function(dbs) {
			let prefix = '/cmpl/';
			for (db of dbs) {
				let name = db.name;
				if (!name.startsWith(prefix)) {
					continue;
				}
				name = name.substr(prefix.length);
				let url = window.location.origin + window.location.pathname + '#workspace=' + name;
				fileList.innerHTML += '<li class="project" onclick="params.update({workspace: \'' + name + '\'});">' + name +
					'<button class="right" onclick="event.stopPropagation(); rmWorkspace(\'' + name + '\')" title="Remove workspace">-</button>' +
					'</li>'
			}
		}).then(function() {
			for (let demo in projects) {
				fileList.innerHTML += '<li class="project" onclick="params.update(toProject(projects[this.textContent]));">' + demo + '</li>'
			}
		});
	} else {
		terminal.append('Failed to list workspaces, indexedDB.databases not available');
		for (let demo in projects) {
			fileList.innerHTML += '<li class="project" onclick="params.update(toProject(projects[this.textContent]));">' + demo + '</li>'
		}
	}
}());

function toProject(proj) {
	let project = {};

	// set libraries to be used
	if (proj.libs != null) {
		let libs = [];
		for (let lib of proj.libs) {
			project[lib] = '';
		}
	}

	// set files to be downloaded
	if (proj.files != null) {
		let files = [];
		for (let file of proj.files) {
			if (file.startsWith(proj.base)) {
				file = file.substring(proj.base.length, file.length);
			}
			let base = (proj.root || '') + (proj.base || '');
			if (!base.endsWith('/')) {
				base += '/';
			}
			files.push({
				path: file,
				url: base + file
			});
		}
		project.project = btoa(JSON.stringify(files));
	}

	// set file to be opened after project is loaded
	if (proj.main != null) {
		let main = proj.main;
		if (main.startsWith(proj.base)) {
			main = main.substring(proj.base.length, main.length);
		}
		project.file = main;
	}

	return project;
}
