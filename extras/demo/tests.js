let projects = {
	'cmplStd/tests' : {
		base: 'cmplStd/test/',
		main: 'cmplStd/test/test.ci',
		files: [
			'lang/emit.ci',
			'lang/function.ci',
			'lang/init.array.ci',
			'lang/init.member.ci',
			'lang/init.method.ci',
			'lang/init.reference.ci',
			'lang/init.variable.ci',
			'lang/inlineMacros.ci',
			'lang/overload.inline.ci',
			'lang/recPacking.ci',
			'lang/recUnion.ci',
			'lang/reflect.ci',
			'lang/stmt.for.ci',
			'lang/stmt.if.ci',
			'lang/useOperator.ci',
			'std/math.Bits.ci',
			'std/math.Complex.ci',
			'std/memory.ci',
			'std/number.ci',
			'std/test.math.ci',
			'std/test.trigonometry.ci',
			'std/tryExec.ci',
			'demo/BitwiseArithmetic.ci',
			'test.ci'
		]
	},
	'cmplGfx/tests' : {
		base: 'cmplGfx/test/',
		main: 'cmplGfx/test/testGfx.ci',
		files: [
			'asset/font/Modern-1.fnt',
			'asset/image/david.png',
			'asset/image/forest.png',
			'asset/image/heightmap.png',
			'asset/image/texture.png',
			'asset/image/texture_nature_01.png',
			'asset/mesh/teapot.obj',
			'demo/StarField.ci',
			'demo/FloodIt.ci',
			'demo/RayTracer.ci',
			'2d.ImageDiff.ci',
			'2d.L-System.ci',
			'2d.draw.bezier.ci',
			'2d.draw.image.ci',
			'3d.draw.mesh.ci',
			'3d.height.map.ci',
			'3d.parametric.ci',
			'fx.color.blend.ci',
			'fx.color.lookup-ease.ci',
			'fx.color.lookup-levels.ci',
			'fx.color.lookup-manual.ci',
			'fx.color.lookup.ci',
			'fx.color.matrix.ci',
			'fx.color.select-hsl.ci',
			'fx.histogram.ci',
			'fx.image.blur.ci',
			'fx.image.transform.ci',
			'pd.blob.ci',
			'pd.complexPlane.ci',
			'pd.yinyang.ci',
			'testGfx.ci',
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
		files: [
			'asset/font/Modern-1.fnt',
			'asset/image/forest.png',
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
