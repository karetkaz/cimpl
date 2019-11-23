## try execute
<iframe frameborder="0" width="100%" height="100" onload="onload=function(){height=contentDocument.body.scrollHeight}; src='/Cmpl/demo.html#theme=light&show=inline&exec=-debug/P&X=-stdin&file=example.ci&content='+btoa(textContent.trim()+'\n');">
void noError(pointer ptr) {
}
void stackOverflow(pointer ptr) {
	byte data[8192];		// speed up the overflow allocating 8 kb memory in each call
	stackOverflow(ptr);
}
void divisionByZero(pointer args) {
	int value = 3 / 0;
}
void abortExecution(pointer args) {
	struct NotEquals {
		char message[*];
		int expected;
		int returned;
	}
	NotEquals details = {
		message: "assertion failed";
		expected: 97;
		returned: 77;
	};
	abort("fatal error", details);
}
void invalidInstruction(pointer args) {
	emit(load.z32, ret);
}
int tryExecErr0 = tryExec(null, noError);
int tryExecErr1 = tryExec(null, null);
int tryExecErr2 = tryExec(null, stackOverflow);
int tryExecErr3 = tryExec(null, divisionByZero);
int tryExecErr4 = tryExec(null, invalidInstruction);
int tryExecErr6 = tryExec(null, abortExecution);
</iframe>

## gfx Yinyang
<iframe frameborder="0" width="100%" height="100" onload="height=contentDocument.body.scrollHeight" src="/Cmpl/demoGfx.html#theme=light&show=inline&X=-stdin&file=example.ci&content=Ly8gcHJvY2VkdXJhbCBkcmF3IGRlbW8gfiBZaW55YW5nCi8vIGFkYXB0ZWQgZnJvbSBodHRwczovL3d3dy5zaGFkZXJ0b3kuY29tL3ZpZXcvNGRHM0RWCgp2ZWM0ZiB5aW55YW5nKHZlYzRmIGluKSB7CglmbG9hdCB0aW1lID0gaW4udzsKCWZsb2F0IG54ID0gMSAtIDIgKiBpbi54OwoJZmxvYXQgbnkgPSAxIC0gMiAqIGluLnk7CglmbG9hdCB4ID0gbnggKiBmbG9hdC5zaW4odGltZSkgLSBueSAqIGZsb2F0LmNvcyh0aW1lKTsKCWZsb2F0IHkgPSBueCAqIGZsb2F0LmNvcyh0aW1lKSArIG55ICogZmxvYXQuc2luKHRpbWUpOwoJZmxvYXQgaCA9IHgqeCArIHkqeTsKCWlmIChoIDwgMS4pIHsKCQlmbG9hdCBkID0gTWF0aC5hYnMoeSkgLSBoOwoJCWZsb2F0IGEgPSBkIC0gMC4yMzsKCQlmbG9hdCBiID0gaCAtIDEuMDA7CgkJZmxvYXQgYyA9IE1hdGguc2lnbihhICogYiAqICh5ICsgeCArICh5IC0geCkgKiBNYXRoLnNpZ24oZCkpKTsKCgkJYyA9IE1hdGgubGVycChNYXRoLnNtb290aCgwLjk4ZiwgMS4wMGYsIGgpLCBjLCAwLjBmKTsKCQljID0gTWF0aC5sZXJwKE1hdGguc21vb3RoKDEuMDBmLCAxLjAyZiwgaCksIGMsIDEuMGYpOwoJCXJldHVybiB2ZWM0ZihmbG9hdChjKSk7Cgl9CglyZXR1cm4gdmVjNGYoZmxvYXQoMSkpOwp9CgoKc2hvdyg1MTIsIDUxMiwgMTI4LCB5aW55YW5nKTsK"></iframe>
