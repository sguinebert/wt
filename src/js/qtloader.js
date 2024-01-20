R"""(
function QtLoader(config)
{
    // The Emscripten module and module configuration object. The module
    // object is created in completeLoadEmscriptenModule().
    self.module = undefined;
    self.moduleConfig = {};

    // Qt properties. These are propagated to the Emscripten module after
    // it has been created.
    self.qtCanvasElements = undefined;
    self.qtFontDpi = 96;

    function webAssemblySupported() {
        return typeof WebAssembly !== "undefined"
    }

    function webGLSupported() {
        // We expect that WebGL is supported if WebAssembly is; however
        // the GPU may be blacklisted.
        try {
            var canvas = document.createElement("canvas");
            return !!(  (canvas.getContext("webgl") || canvas.getContext("experimental-webgl")));
        } catch (e) {
            return false;
        }
    }

    function canLoadQt() {
        // The current Qt implementation requires WebAssembly (asm.js is not in use),
        // and also WebGL (there is no raster fallback).
        return webAssemblySupported() && webGLSupported();
    }

    function removeChildren(element) {
        while (element.firstChild) element.removeChild(element.firstChild);
    }

    function createCanvas() {
        var canvas = document.createElement("canvas");
        canvas.className = "QtCanvas";
        canvas.style.height = "100%";
        canvas.style.width = "100%";

        // Set contentEditable in order to enable clipboard events; hide the resulting focus frame.
        canvas.contentEditable = true;
        canvas.style.outline = "0px solid transparent";
        canvas.style.caretColor = "transparent";
        canvas.style.cursor = "default";

        return canvas;
    }

    // Set default state handler functions and create canvases if needed
    if (config.containerElements !== undefined) {

        config.canvasElements = config.containerElements.map(createCanvas);

        config.showError = config.showError || function(errorText, container) {
            removeChildren(container);
            var errorTextElement = document.createElement("text");
            errorTextElement.className = "QtError"
            errorTextElement.innerHTML = errorText;
            return errorTextElement;
        }

        config.showLoader = config.showLoader || function(loadingState, container) {
            removeChildren(container);
            var loadingText = document.createElement("text");
            loadingText.className = "QtLoading"
            loadingText.innerHTML = '<p><center> ${loadingState}...</center><p>';
            return loadingText;
        };

        config.showCanvas = config.showCanvas || function(canvas, container) {
            removeChildren(container);
        }

        config.showExit = config.showExit || function(crashed, exitCode, container) {
            if (!crashed)
                return undefined;

            removeChildren(container);
            var fontSize = 54;
            var crashSymbols = ["\u{1F615}", "\u{1F614}", "\u{1F644}", "\u{1F928}", "\u{1F62C}",
                                "\u{1F915}", "\u{2639}", "\u{1F62E}", "\u{1F61E}", "\u{1F633}"];
            var symbolIndex = Math.floor(Math.random() * crashSymbols.length);
            var errorHtml = `<font size='${fontSize}'> ${crashSymbols[symbolIndex]} </font>`
            var errorElement = document.createElement("text");
            errorElement.className = "QtExit"
            errorElement.innerHTML = errorHtml;
            return errorElement;
        }
    }

    config.restartMode = config.restartMode || "DoNotRestart";
    config.restartLimit = config.restartLimit || 10;

    if (config.stdoutEnabled === undefined) config.stdoutEnabled = true;
    if (config.stderrEnabled === undefined) config.stderrEnabled = true;

    // Make sure config.path is defined and ends with "/" if needed
    if (config.path === undefined)
        config.path = "";
    if (config.path.length > 0 && !config.path.endsWith("/"))
        config.path = config.path.concat("/");

    if (config.environment === undefined)
        config.environment = {};

    var publicAPI = {};
    publicAPI.webAssemblySupported = webAssemblySupported();
    publicAPI.canLoadQt = canLoadQt();
    publicAPI.canLoadApplication = canLoadQt();
    publicAPI.status = undefined;
    publicAPI.loadEmscriptenModule = loadEmscriptenModule;
    publicAPI.addCanvasElement = addCanvasElement;
    publicAPI.removeCanvasElement = removeCanvasElement;
    publicAPI.resizeCanvasElement = resizeCanvasElement;
    publicAPI.setFontDpi = setFontDpi;
    publicAPI.fontDpi = fontDpi;
    publicAPI.module = module;

    self.restartCount = 0;

    function fetchResource(filePath) {
        var fullPath = config.path + filePath;
        return fetch(fullPath).then(function(response) {
            if (!response.ok) {
                self.error = response.status + " " + response.statusText + " " + response.url;
                setStatus("Error");
                return Promise.reject(self.error)
            } else {
                return response;
            }
        });
    }

    function fetchText(filePath) {
        return fetchResource(filePath).then(function(response) {
            return response.text();
        });
    }

    function fetchThenCompileWasm(response) {
        return response.arrayBuffer().then(function(data) {
            self.loaderSubState = "Compiling";
            setStatus("Loading") // trigger loaderSubState update
            return WebAssembly.compile(data);
        });
    }

    function fetchCompileWasm(filePath) {
        return fetchResource(filePath).then(function(response) {
            if (typeof WebAssembly.compileStreaming !== "undefined") {
                self.loaderSubState = "Downloading/Compiling";
                setStatus("Loading");
                return WebAssembly.compileStreaming(response).catch(function(error) {
                    // compileStreaming may/will fail if the server does not set the correct
                    // mime type (application/wasm) for the wasm file. Fall back to fetch,
                    // then compile in this case.
                    return fetchThenCompileWasm(response);
                });
            } else {
                // Fall back to fetch, then compile if compileStreaming is not supported
                return fetchThenCompileWasm(response);
            }
        });
    }

    function loadEmscriptenModule(applicationName) {
        // Loading in qtloader.js goes through four steps:
        // 1) Check prerequisites
        // 2) Download resources
        // 3) Configure the emscripten Module object
        // 4) Start the emcripten runtime, after which emscripten takes over

        // Check for Wasm & WebGL support; set error and return before downloading resources if missing
        if (!webAssemblySupported()) {
            self.error = "Error: WebAssembly is not supported"
            setStatus("Error");
            return;
        }

        // Continue waiting if loadEmscriptenModule() is called again
        if (publicAPI.status === "Loading")
            return;
        self.loaderSubState = "Downloading";
        setStatus("Loading");

        // Fetch emscripten generated javascript runtime
        var emscriptenModuleSource = undefined
        var emscriptenModuleSourcePromise = fetchText(applicationName + ".js").then(function(source) {
            emscriptenModuleSource = source
        });

        // Fetch and compile wasm module
        var wasmModule = undefined;
        var wasmModulePromise = fetchCompileWasm(applicationName + ".wasm").then(function (module) {
            wasmModule = module;
        });
        
        // Wait for all resources ready
        Promise.all([emscriptenModuleSourcePromise, wasmModulePromise]).then(function(){
            completeLoadEmscriptenModule(applicationName, emscriptenModuleSource, wasmModule);
        }).catch(function(error) {
            self.error = error;
            setStatus("Error");
        });
    }

    function completeLoadEmscriptenModule(applicationName, emscriptenModuleSource, wasmModule) {

        // The wasm binary has been compiled into a module during resource download,
        // and is ready to be instantiated. Define the instantiateWasm callback which
        // emscripten will call to create the instance.
        self.moduleConfig.instantiateWasm = function(imports, successCallback) {
            WebAssembly.instantiate(wasmModule, imports).then(function(instance) {            	
                successCallback(instance, wasmModule);
            }, function(error) {
                self.error = error;
                setStatus("Error");
            });
            return {};
        };

        self.moduleConfig.locateFile = self.moduleConfig.locateFile || function(filename) {
            return config.path + filename;
        };

        // Attach status callbacks
        self.moduleConfig.setStatus = self.moduleConfig.setStatus || function(text) {
            // Currently the only usable status update from this function
            // is "Running..."
            if (text.startsWith("Running"))
                setStatus("Running");
        };
        self.moduleConfig.monitorRunDependencies = self.moduleConfig.monitorRunDependencies || function(left) {
          //  console.log("monitorRunDependencies " + left)
        };

        // Attach standard out/err callbacks.
        self.moduleConfig.print = self.moduleConfig.print || function(text) {
            if (config.stdoutEnabled)
                console.log(text)
        };
        self.moduleConfig.printErr = self.moduleConfig.printErr || function(text) {
            // Filter out OpenGL getProcAddress warnings. Qt tries to resolve
            // all possible function/extension names at startup which causes
            // emscripten to spam the console log with warnings.
            if (text.startsWith !== undefined && text.startsWith("bad name in getProcAddress:"))
                return;

            if (config.stderrEnabled)
                console.log(text)
        };

        // Error handling: set status to "Exited", update crashed and
        // exitCode according to exit type.
        // Emscripten will typically call printErr with the error text
        // as well. Note that emscripten may also throw exceptions from
        // async callbacks. These should be handled in window.onerror by user code.
        self.moduleConfig.onAbort = self.moduleConfig.onAbort || function(text) {
            publicAPI.crashed = true;
            publicAPI.exitText = text;
            setStatus("Exited");
        };
        self.moduleConfig.quit = self.moduleConfig.quit || function(code, exception) {

            // Emscripten (and Qt) supports exiting from main() while keeping the app
            // running. Don't transition into the "Exited" state for clean exits.
            if (code === 0)
                return;

            if (exception.name === "ExitStatus") {
                // Clean exit with code
                publicAPI.exitText = undefined
                publicAPI.exitCode = code;
            } else {
                publicAPI.exitText = exception.toString();
                publicAPI.crashed = true;
            }
            setStatus("Exited");
        };

        self.moduleConfig.preRun = self.moduleConfig.preRun || []
        self.moduleConfig.preRun.push(function(module) {
            // Set environment variables
            for (var [key, value] of Object.entries(config.environment)) {
                module.ENV[key.toUpperCase()] = value;
            }
            // Propagate Qt module properties
            module.qtCanvasElements = self.qtCanvasElements;
            module.qtFontDpi = self.qtFontDpi;
        });

        self.moduleConfig.mainScriptUrlOrBlob = new Blob([emscriptenModuleSource], {type: 'text/javascript'});

        self.qtCanvasElements = config.canvasElements;

        config.restart = function() {

            // Restart by reloading the page. This will wipe all state which means
            // reload loops can't be prevented.
            if (config.restartType === "ReloadPage") {
                location.reload();
            }

            // Restart by readling the emscripten app module.
            ++self.restartCount;
            if (self.restartCount > config.restartLimit) {
                self.error = "Error: This application has crashed too many times and has been disabled. Reload the page to try again."
                setStatus("Error");
                return;
            }
            loadEmscriptenModule(applicationName);
        };

        publicAPI.exitCode = undefined;
        publicAPI.exitText = undefined;
        publicAPI.crashed = false;

        // Load the Emscripten application module. This is done by eval()'ing the
        // javascript runtime generated by Emscripten, and then calling
        // createQtAppInstance(), which was added to the global scope.
        eval(emscriptenModuleSource);
        createQtAppInstance(self.moduleConfig).then(function(module) {
            self.module = module;
            console.log('module compiled', module);
            module.hello();
        });
    }

    function setErrorContent() {
        if (config.containerElements === undefined) {
            if (config.showError !== undefined)
                config.showError(self.error);
            return;
        }

        for (container of config.containerElements) {
            var errorElement = config.showError(self.error, container);
            container.appendChild(errorElement);
        }
    }

    function setLoaderContent() {
        if (config.containerElements === undefined) {
            if (config.showLoader !== undefined)
                config.showLoader(self.loaderSubState);
            return;
        }

        for (container of config.containerElements) {
            var loaderElement = config.showLoader(self.loaderSubState, container);
            container.appendChild(loaderElement);
        }
    }

    function setCanvasContent() {
        if (config.containerElements === undefined) {
            if (config.showCanvas !== undefined)
                config.showCanvas();
            return;
        }

        for (var i = 0; i < config.containerElements.length; ++i) {
            var container = config.containerElements[i];
            var canvas = config.canvasElements[i];
            config.showCanvas(canvas, container);
            container.appendChild(canvas);
        }
    }

    function setExitContent() {

        // publicAPI.crashed = true;

        if (publicAPI.status !== "Exited")
            return;

        if (config.containerElements === undefined) {
            if (config.showExit !== undefined)
                config.showExit(publicAPI.crashed, publicAPI.exitCode);
            return;
        }

        if (!publicAPI.crashed)
            return;

        for (container of config.containerElements) {
            var loaderElement = config.showExit(publicAPI.crashed, publicAPI.exitCode, container);
            if (loaderElement !== undefined)
                container.appendChild(loaderElement);
        }
    }

    var committedStatus = undefined;
    function handleStatusChange() {
        if (publicAPI.status !== "Loading" && committedStatus === publicAPI.status)
            return;
        committedStatus = publicAPI.status;

        if (publicAPI.status === "Error") {
            setErrorContent();
        } else if (publicAPI.status === "Loading") {
            setLoaderContent();
        } else if (publicAPI.status === "Running") {
            setCanvasContent();
        } else if (publicAPI.status === "Exited") {
            if (config.restartMode === "RestartOnExit" ||
                config.restartMode === "RestartOnCrash" && publicAPI.crashed) {
                    committedStatus = undefined;
                    config.restart();
            } else {
                setExitContent();
            }
        }

        // Send status change notification
        if (config.statusChanged)
            config.statusChanged(publicAPI.status);
    }

    function setStatus(status) {
        if (status !== "Loading" && publicAPI.status === status)
            return;
        publicAPI.status = status;

    }

    function addCanvasElement(element) {
        if (publicAPI.status === "Running")
            self.module.qtAddCanvasElement(element);
        else
            console.log("Error: addCanvasElement can only be called in the Running state");
    }

    function removeCanvasElement(element) {
        if (publicAPI.status === "Running")
            self.module.qtRemoveCanvasElement(element);
        else
            console.log("Error: removeCanvasElement can only be called in the Running state");
    }

    function resizeCanvasElement(element) {
        if (publicAPI.status === "Running")
            self.module.qtResizeCanvasElement(element);
    }

    function setFontDpi(dpi) {
        self.qtFontDpi = dpi;
        if (publicAPI.status === "Running")
            self.qtSetFontDpi(dpi);
    }

    function fontDpi() {
        return self.qtFontDpi;
    }

    function module() {
        return self.module;
    }

    setStatus("Created");

    return publicAPI;
}
)"""
