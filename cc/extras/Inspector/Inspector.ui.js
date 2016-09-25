// 
var data = Instrument();

function loadFile(input, action) {
	var fileName = document.getElementById('file_name');
	if (input.files.length !== 1) {
		action("Single file required.");
		//setActiveTab(null, null, document.getElementById("error"));
		throw "Single file required."
	}
	setActiveTab(null, null, document.getElementById("loading"));
	data.loadFile(input.files[0], function(error) {
		fileName.innerText = input.files[0].name;
		action(error);
		/*if (error == null) {

			setActiveTab(null, null, document.getElementById("loaded"));
		}
		else {
			setActiveTab(null, null, document.getElementById("error"));
			throw error;
		}*/
	});
}

function setActiveTab(tab, ext, node, contentWriter) {
	var i, element;
	var tabs = document.getElementById('tabs').children;
	var elements = document.getElementById('content').children;
	for (i = 0; i < tabs.length; i++) {
		element = tabs[i];
		if (element === tab) {
			element.classList.add('active');
		}
		else if (element === ext) {
			element.classList.add('active');
		}
		else {
			element.classList.remove('active');
		}
	}
	if (node != null) {
		for (i = 0; i < elements.length; i++) {
			element = elements[i];
			if (element === node.parentElement) {
				element.classList.add('active');
			}
			else {
				element.classList.remove('active');
			}
		}
	}
	if (contentWriter != null) {
		var text, args = [];
		for (i = 4; i < arguments.length; ++i) {
			args[i - 4] = arguments[i];
		}
		text = contentWriter.apply(this, args);
		if (text != null && node != null) {
			node.innerText = text;
		}
	}
}

function toJsString(obj, prefix) {
	var objects = [];	// prevent recursion
	function toJsStringRecursive(obj, prefix) {
		var result, inner;
		if (obj === null || obj === undefined) {
			return obj === null ? 'null' : 'undefined';
		}
		if (obj.constructor === Function) {
			//~ return 'function' + obj.toString().match(/\(.*\)/) + ' { ... }';
			return null;
		}
		if (obj.constructor === Object) {
			if (objects.indexOf(obj) >= 0) {
				return null;
			}
			result = '';
			var prefix2 = '';
			if (prefix === null || prefix === undefined) {
				prefix = '';
			}
			else if (prefix !== '') {
				prefix2 = prefix + '  ';
			}
			objects.push(obj);
			for (var key in obj) {
				inner = toJsStringRecursive(obj[key], prefix2);
				if (inner !== null) {
					if (result !== '') {
						result += ', ';
					}
					result += prefix2 + key + ': ' + inner;
				}
			}
			if (prefix !== '' && result.indexOf(prefix) == 0) {
				result += prefix;
			}
			return '{' + result + '}';
		}
		if (obj.constructor === Array) {
			result = '';
			if (prefix === null || prefix === undefined) {
				prefix = '';
			}
			for (var idx = 0; idx < obj.length; idx += 1) {
				inner = toJsStringRecursive(obj[idx], prefix);
				if (inner !== null) {
					if (result !== '') {
						result += ', ';
					}
					result += inner;
				}
			}
			if (prefix !== '' && result.indexOf(prefix) == 0) {
				result += prefix;
			}
			return '[' + result + ']';
		}

		if (obj.constructor === String) {
			return '"' + obj.toString().replace(/\"/g, '\\"') + '"';
		}

		// Boolean, Number, Date, Math, RegExp, Global, ... ???
		return obj.toString();
	}

	return toJsStringRecursive(obj, prefix);
}

// contentWriter for setActiveTab function
function printSymbols() {
	var types = document.getElementById('apiTyp').checked;
	var funcs = document.getElementById('apiFun').checked;
	var vars = document.getElementById('apiVar').checked;
	var defs = document.getElementById('apiDef').checked;
	var opcodes = document.getElementById('apiOpc').checked;
	return data.api.getFiltered(types, funcs, vars, defs, opcodes);
}

// contentWriter for setActiveTab function
function printSamples() {
	var samples = data.getSamples();
	return toJsString(samples, '\n');
}

// build callChart and callTree
function displayCallTree(chartDiv, treeDiv, focusedNode) {
	var samples = data.getSamples();
	var tree = (focusedNode && focusedNode.__proto__);
	if (tree != null) {
		tree = {
			enter: tree.enter,
			leave: tree.leave,
			calltree: [tree]
		}
	}

	tree = extendCallTree(tree || samples);

	function extendCallTree(obj, parent) {
		var i, ct, result = Object.create(obj);
		if (obj != null && obj.calltree != null) {
			ct = [];
			for (i = 0; i < obj.calltree.length; ++i) {
				/* TODO: exclude very short calls
				var node = obj.calltree[i];
				var time = node.total * 100;
				time /= samples.leave - samples.enter;
				if (time < 0.01) {
					continue;
				}
				*/
				ct.push(extendCallTree(obj.calltree[i], result));
			}
			Object.defineProperty(result, 'calltree', {value: ct});
		}
		Object.defineProperty(result, 'parent', {value: parent});
		return result;
	}

	function walkCallTree(node, depth, action) {
		if (node.calltree != null) {
			for (var i = 0; i < node.calltree.length; ++i) {
				var child = node.calltree[i];
				action(child, depth, node);
				walkCallTree(child, depth + 1, action);
			}
		}
	}

	if (chartDiv != null) {
		var chartWidth = chartDiv.clientWidth;
		var timeScale = chartWidth / (tree.leave - tree.enter);
		var heightScale = 2 + parseFloat(getComputedStyle(chartDiv).fontSize);
		var maxDepth = 100;//(chartDiv.clientHeight) / heightScale - 1;
		var minTime = 0;//(tree.leave - tree.enter) / 5000;	// 0.05%

		treeDiv.innerHTML = '';
		chartDiv.innerHTML = '';

		var row = document.createElement("tr");
		var func, hits, selfTime, totalTime;
		row.appendChild(hits = document.createElement("th"));
		row.appendChild(selfTime = document.createElement("th"));
		row.appendChild(totalTime = document.createElement("th"));
		row.appendChild(func = document.createElement("th"));
		hits.innerText = "Hits";
		hits.width = "30px";
		selfTime.innerText = "Self Time";
		selfTime.width = "70px";
		totalTime.innerText = "Total Time";
		totalTime.width = "75px";
		func.innerText = "Function";
		treeDiv.appendChild(row);

		treeDiv.expandTree = function(expand, all) {
			tree.calltree[0].expandTree(expand, all);
		};

		walkCallTree(tree, 0, function(node, depth) {
			function toMs(ticks) {
				// return ticks + ' ticks';
				return (ticks * 1000 / samples.ticksPerSec).toFixed(2) + ' ms';
			}
			var chartRow, treeRow, expandable = false;
			var left = timeScale * (node.enter - tree.enter);
			var right = timeScale * (tree.leave - node.leave);

			var funct = node.func || { name: '??' };
			var sample = samples.samples[funct.offs] || {};	// make everything undefined if function is not found

			var globalTime = samples.leave - samples.enter;

			var callSelfCost = (node.self * 100 / globalTime).toFixed(2) + '%';
			var callTotalCost = (node.total * 100 / globalTime).toFixed(2) + '%';
			var sampleSelfCost = (sample.self * 100 / globalTime).toFixed(2) + '%';
			var sampleTotalCost = (sample.total * 100 / globalTime).toFixed(2) + '%';

			var callSelfTime = toMs(node.self);
			var callTotalTime = toMs(node.total);
			var sampleSelfTime = toMs(sample.self);
			var sampleTotalTime = toMs(sample.total);

			var funcPos = ' <@' + funct.offs + '>';
			if (funct.file && funct.line) {
				funcPos = '(' + funct.file + ':' + funct.line + ')';
			}
			var title = 'Function: ' + funct.name + funcPos;
			if (sample.hits > 1) {
				title += '\nHit Count: ' + node.hits + ' / ' + sample.hits;
				title += '\nSelf Cost: ' + callSelfCost + ' / ' + sampleSelfCost;
				title += '\nSelf Time: ' + callSelfTime + ' / ' + sampleSelfTime;
				title += '\nTotal Cost: ' + callTotalCost + ' / ' + sampleTotalCost;
				title += '\nTotal Time: ' + callTotalTime + ' / ' + sampleTotalTime;
			}
			else {
				title += '\nHit Count: ' + sample.hits;
				title += '\nSelf Cost: ' + sampleSelfCost;
				title += '\nSelf Time: ' + sampleSelfTime;
				title += '\nTotal Cost: ' + sampleTotalCost;
				title += '\nTotal Time: ' + sampleTotalTime;
			}
			title += '\nExecution Time: ' + toMs(node.enter - samples.enter) + ' - ' + toMs(node.leave - samples.enter);
			title += '\nMemory Usage: ' + node.memory;
			title += '\n' + funct.proto;

			// add to call chartDiv
			if (depth < maxDepth && timeScale * node.total > .3) {
				chartRow = document.createElement("div");
				chartDiv.appendChild(chartRow);

				chartRow.innerText = '\xa0' + funct.name;

				chartRow.style.left = left;
				chartRow.style.right = right;
				chartRow.style.top = depth * heightScale + 1;
				chartRow.style.height = heightScale - 1;
				chartRow.title = title;
				//node.htmlChartRow = chartRow;
				chartRow.onclick = function() {
					node.showTree(true, true);
					var tooltip = document.getElementById('tooltip');
					tooltip.innerText = title;
					tooltip.classList.remove('gone');
				};
				chartRow.ondblclick = function() {
					displayCallTree(chartDiv, treeDiv, node);
				};
			}

			// add to call tree
			if (node.total > minTime) {
				var func, hits, selfTime, totalTime;
				treeRow = document.createElement("tr");
				treeRow.appendChild(hits = document.createElement("td"));
				treeRow.appendChild(selfTime = document.createElement("td"));
				treeRow.appendChild(totalTime = document.createElement("td"));
				treeRow.appendChild(func = document.createElement("td"));

				hits.innerText = node.hits;
				//selfTime.innerText = callSelfCost;
				//totalTime.innerText = callTotalCost;
				// use local percentages
				selfTime.innerText = (node.self * 100 / (tree.leave - tree.enter)).toFixed(2) + '%';
				totalTime.innerText = (node.total * 100 / (tree.leave - tree.enter)).toFixed(2) + '%';

				node.htmlTreeRow = treeRow;		// add a reference to the table row

				// check if the node has sub-calls
				//expandable = node.calltree && node.calltree.length > 0;
				expandable = node.calltree != null;

				node.isExpanded = function() {
					if (func.classList.contains("expanded")) {
						if (this.parent.isExpanded == null) {
							return true;
						}
						return this.parent.isExpanded();
					}
					return false;
				};

				node.showTree = function(show, highlight) {
					var n;
					if (show == false) {
						treeRow.style.display = 'none';
						return;
					}

					// show parent nodes
					for (n = node; n != null; n = n.parent) {
						if (n.htmlTreeRow != null) {
							n.htmlTreeRow.style.display = '';
						}
					}
					if (highlight) {
						treeRow.scrollIntoView(false);
						treeRow.classList.add('hover');
						setTimeout(function() {
							treeRow.classList.remove('hover');
						}, 500);
					}

				};
				node.expandTree = function(expand, recursive, expandOnly) {
					if (expandable) {
						// set expanded or collapsed
						func.classList.remove("expanded");
						func.classList.remove("collapsed");
						func.classList.add(expand ? "expanded" : "collapsed");
					}

					if (expandOnly !== true) {
						walkCallTree(node, 0, function(node, depth, parent) {
							node.showTree(expand && parent.isExpanded());
							if (recursive && node.expandTree) {
								node.expandTree(expand, recursive, true);
							}
						});
					}
				};
				if (expandable) {
					// expanded by default.
					func.classList.add("expanded");
					func.onclick = function(event) {
						var expand = func.classList.contains("collapsed");
						node.expandTree(expand, event.ctrlKey);
					}
				}

				func.appendChild(document.createTextNode(funct.proto));
				func.style.paddingLeft = depth + 'em';
				treeRow.title = title;
				treeRow.onmouseenter = function() {
					node.highlightInChart(true);
				};
				treeRow.onmouseleave = function() {
					node.highlightInChart(false);
				};
				treeRow.onclick = function() {
					var tooltip = document.getElementById('tooltip');
					tooltip.innerText = title;
					tooltip.classList.remove('gone');
				};
				treeDiv.appendChild(treeRow);
			}

			node.highlightInChart = function(active) {
				if (chartRow == null) {
					return;
				}
				if (active) {
					chartRow.classList.add('hover');
				}
				else {
					chartRow.classList.remove('hover');
				}
			};
		});
	}
}
