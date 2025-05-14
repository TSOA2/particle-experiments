#include <main.hpp>

void ParticleSet::init(SDL_GPUDevice *gpuDevice)
{
	std::size_t numParticles = NPARTICLES;

	this->gpuDevice = gpuDevice;
	std::random_device device;
	std::mt19937 rng(device());

	std::uniform_real_distribution<float> xdisr(0.0f, PARTICLE_BOX_X);
	std::uniform_real_distribution<float> ydisr(0.0f, PARTICLE_BOX_Y);
	std::uniform_real_distribution<float> zdisr(0.0f, PARTICLE_BOX_Z);
	std::uniform_real_distribution<float> mdisr(MASS_LOW, MASS_HIGH);

	particles.reserve(numParticles);
	for (std::size_t i = 0; i < numParticles; i++) {
		Particle particle = {
			.x = xdisr(rng),
			.y = ydisr(rng),
			.z = zdisr(rng),
			.mass = 1000,
			.vx = 0,
			.vy = 0,
			.vz = 0
		};

		particles.push_back(particle);
	}

	const std::size_t bsize = numParticles * sizeof(ParticleSet::Particle);
	SDL_GPUBufferCreateInfo bufferCreateInfo{};
	bufferCreateInfo.usage =
		SDL_GPU_BUFFERUSAGE_GRAPHICS_STORAGE_READ |
		SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ  |
		SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_WRITE;
	bufferCreateInfo.size = bsize;

	particleBuffer = SDL_CreateGPUBuffer(gpuDevice, &bufferCreateInfo);
	if (!particleBuffer)
		throw SDLError("failed to create particle buffer");

	SDL_GPUTransferBufferCreateInfo transferCreateInfo{};
	transferCreateInfo.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
	transferCreateInfo.size = bsize;

	transferBuffer = SDL_CreateGPUTransferBuffer(gpuDevice, &transferCreateInfo);
	if (!transferBuffer)
		throw SDLError("failed to create particle transfer buffer");
}

void ParticleSet::upload()
{
	SDL_GPUCommandBuffer *cmdBuf = SDL_AcquireGPUCommandBuffer(gpuDevice);
	if (!cmdBuf) {
		log(SDL_LOG_PRIORITY_CRITICAL, "failed to acquire command buffer to upload particles: %s", SDL_GetError());
		return ;
	}

	auto particlesGPU = static_cast<ParticleSet::Particle *>(SDL_MapGPUTransferBuffer(gpuDevice, transferBuffer, false));
	if (!particlesGPU) {
		log(SDL_LOG_PRIORITY_CRITICAL, "failed to map particle transfer buffer: %s", SDL_GetError());
		return ;
	}

	std::size_t size = particles.size();
	std::copy(particles.begin(), particles.end(), particlesGPU);
	SDL_UnmapGPUTransferBuffer(gpuDevice, transferBuffer);

	SDL_GPUCopyPass *copyPass = SDL_BeginGPUCopyPass(cmdBuf);

	SDL_GPUTransferBufferLocation location{};
	location.transfer_buffer = transferBuffer;

	SDL_GPUBufferRegion region{};
	region.buffer = particleBuffer;
	region.size = size * sizeof(ParticleSet::Particle);

	SDL_UploadToGPUBuffer(copyPass, &location, &region, false);
	SDL_EndGPUCopyPass(copyPass);

	SDL_SubmitGPUCommandBuffer(cmdBuf);
}

SDL_GPUBuffer *ParticleSet::getBuffer() const
{
	return particleBuffer;
}

std::size_t ParticleSet::getNum() const
{
	return particles.size();
}

void ParticleSet::deinit()
{
	if (transferBuffer)
		SDL_ReleaseGPUTransferBuffer(gpuDevice, transferBuffer);

	if (particleBuffer)
		SDL_ReleaseGPUBuffer(gpuDevice, particleBuffer);
}

void GPUNewtonApp::loadDevice()
{
	const auto desiredShaderFormats =
		SDL_GPU_SHADERFORMAT_SPIRV |
		SDL_GPU_SHADERFORMAT_MSL;

#ifdef NDEBUG
	const bool debugMode = false;
#else
	const bool debugMode = true;
#endif

	gpuDevice = SDL_CreateGPUDevice(desiredShaderFormats, debugMode, nullptr);
	if (!gpuDevice)
		throw SDLError("failed to create GPU device");

#ifndef NDEBUG
	log(SDL_LOG_PRIORITY_INFO, "using GPU device driver: %s", SDL_GetGPUDeviceDriver(gpuDevice));
#endif

	if (!SDL_ClaimWindowForGPUDevice(gpuDevice, window))
		throw SDLError("unable to set GPU device window");
}

template <class T>
void GPUNewtonApp::loadShaderData(std::string &&name, T &shaderData)
{
	std::string fileName(name);

	SDL_GPUShaderFormat fmt = SDL_GetGPUShaderFormats(gpuDevice);
	const char *entryPoint = "main";
	if (fmt & SDL_GPU_SHADERFORMAT_MSL) {
		fileName.append(".msl");
		entryPoint = "main0";
		fmt = SDL_GPU_SHADERFORMAT_MSL;
	} else if (fmt & SDL_GPU_SHADERFORMAT_SPIRV) {
		fileName.append(".spv");
		fmt = SDL_GPU_SHADERFORMAT_SPIRV;
	} else
		throw std::runtime_error("shader formats not supported on this machine");

	std::size_t codeSize;
	void *data = SDL_LoadFile(fileName.data(), &codeSize);
	if (!data)
		throw SDLError("failed to read shader file");

	shaderData.code_size = codeSize;
	shaderData.code = static_cast<const std::uint8_t *>(data);
	shaderData.entrypoint = entryPoint;
	shaderData.format = fmt;
}

SDL_GPUShader *GPUNewtonApp::loadShader(
	std::string_view fname,
	std::uint32_t numSamplers,
	std::uint32_t numStorageTextures,
	std::uint32_t numStorageBuffers,
	std::uint32_t numUniformBuffers)
{
	std::string fileName(fname);
	SDL_GPUShaderStage stage;
	if (SDL_strstr(fileName.data(), ".vert"))
		stage = SDL_GPU_SHADERSTAGE_VERTEX;
	else if (SDL_strstr(fileName.data(), ".frag"))
		stage = SDL_GPU_SHADERSTAGE_FRAGMENT;
	else
		throw std::runtime_error("invalid shader file extension (vert/frag)");

	SDL_GPUShaderCreateInfo createInfo{};
	loadShaderData(std::move(fileName), createInfo);
	createInfo.stage = stage;
	createInfo.num_samplers = numSamplers;
	createInfo.num_storage_textures = numStorageTextures;
	createInfo.num_storage_buffers = numStorageBuffers;
	createInfo.num_uniform_buffers = numUniformBuffers;
	createInfo.props = 0;

	SDL_GPUShader *shader = SDL_CreateGPUShader(gpuDevice, &createInfo);
	SDL_free((void *) createInfo.code);
	if (!shader)
		throw SDLError("failed to create shader");

	return shader;
}

void GPUNewtonApp::loadPipeline()
{
	SDL_GPUShader *vertShader = loadShader(VERT_SHADER_FNAME, 0, 0, 1, 1);
	SDL_GPUShader *fragShader = loadShader(FRAG_SHADER_FNAME, 0, 0, 0, 0);

	SDL_GPUColorTargetDescription colorDesc{};
	colorDesc.format = SDL_GetGPUSwapchainTextureFormat(gpuDevice, window),
	colorDesc.blend_state = {
		.src_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
		.dst_color_blendfactor = SDL_GPU_BLENDFACTOR_DST_ALPHA,
		.color_blend_op = SDL_GPU_BLENDOP_ADD,
		.src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_SRC_COLOR,
		.dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_DST_COLOR,
		.alpha_blend_op = SDL_GPU_BLENDOP_ADD,
		.enable_blend = true
	};

	SDL_GPUGraphicsPipelineCreateInfo createInfo{};
	createInfo.vertex_shader = vertShader;
	createInfo.fragment_shader = fragShader;
	createInfo.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
	createInfo.rasterizer_state = SDL_GPURasterizerState {
		.fill_mode = SDL_GPU_FILLMODE_FILL
	};
	createInfo.target_info = SDL_GPUGraphicsPipelineTargetInfo {
		.color_target_descriptions = &colorDesc,
		.num_color_targets = 1
	};
	gpuPipeline = SDL_CreateGPUGraphicsPipeline(gpuDevice, &createInfo);
	SDL_ReleaseGPUShader(gpuDevice, vertShader);
	SDL_ReleaseGPUShader(gpuDevice, fragShader);
	if (!gpuPipeline)
		throw SDLError("failed to load graphics pipeline");
}

void GPUNewtonApp::loadComputePipeline()
{
	const auto numSamplers   = 0;
	const auto numROTextures = 0;
	const auto numROBuffers  = 0;
	const auto numRWTextures = 0;
	const auto numRWBuffers  = 1;
	const auto numUniforms   = 0;

	SDL_GPUComputePipelineCreateInfo createInfo{};
	loadShaderData(std::string(COMP_SHADER_FNAME), createInfo);
	createInfo.num_samplers = numSamplers;
	createInfo.num_readonly_storage_textures = numROTextures;
	createInfo.num_readonly_storage_buffers = numROBuffers;
	createInfo.num_readwrite_storage_textures = numRWTextures;
	createInfo.num_readwrite_storage_buffers = numRWBuffers;
	createInfo.num_uniform_buffers = numUniforms;
	createInfo.threadcount_x = 1000;
	createInfo.threadcount_y = 1;
	createInfo.threadcount_z = 1;

	compPipeline = SDL_CreateGPUComputePipeline(gpuDevice, &createInfo);
	SDL_free((void *) createInfo.code);
	if (!compPipeline)
		throw SDLError("failed to load compute pipeline");
}

void GPUNewtonApp::updateProjMatrix(int width, int height)
{
	float aspect =
		static_cast<float>(width) /
		static_cast<float>(height);
	projMatrix = glm::perspective(glm::radians(FOV), aspect, NEAR, FAR);
}

bool GPUNewtonApp::keyPressed(SDL_Keycode key, [[maybe_unused]] bool wait)
{
	return keyboard[SDL_GetScancodeFromKey(key, nullptr)];
}

void GPUNewtonApp::updateCameraMouse(float xrel, float yrel)
{
	static float yaw;
	static float pitch;

	yaw += glm::radians(xrel * MOUSE_SENSITIVITY);
	pitch -= glm::radians(yrel * MOUSE_SENSITIVITY);

	const float topAngle = glm::pi<float>()/2 - 0.1;
	if (pitch > topAngle)
		pitch = topAngle;
	else if (pitch < -topAngle)
		pitch = -topAngle;

	const float x = glm::cos(yaw) * glm::cos(pitch);
	const float y = glm::sin(pitch);
	const float z = glm::sin(yaw) * glm::cos(pitch);
	camLookat = glm::vec3(x, y, z);
}

void GPUNewtonApp::updateCameraPos(const Keyboard &keyboard)
{
	/* Where the camera is */
	{
		const float multiplier =
			static_cast<float>(delta / std::chrono::seconds(1)) *
			CAMERA_SPEED;

		if (keyPressed(SDLK_W, false))
			camPosition += camLookat * multiplier;

		if (keyPressed(SDLK_S, false))
			camPosition -= camLookat * multiplier;
	}
}

void GPUNewtonApp::createCombined(glm::mat4 &combined)
{
	glm::mat4 viewMatrix = glm::lookAt(camPosition, camPosition + camLookat, glm::vec3(0.0, 1.0, 0.0));
	/* Rendering billboards, so we can remove the majority of the viewMatrix :)
	viewMatrix[0][0] = 1;
	viewMatrix[0][1] = 0;
	viewMatrix[0][2] = 0;
	viewMatrix[1][0] = 0;
	viewMatrix[1][1] = 1;
	viewMatrix[1][2] = 0;
	viewMatrix[2][0] = 0;
	viewMatrix[2][1] = 0;
	viewMatrix[2][2] = 1;
	*/

	combined = projMatrix * viewMatrix;
}

GPUNewtonApp::GPUNewtonApp(std::string_view title, int width, int height)
{
	const SDL_InitFlags initFlags = SDL_INIT_VIDEO;
	const SDL_WindowFlags windowFlags = SDL_WINDOW_HIDDEN;

	if (!SDL_Init(initFlags))
		throw SDLError("failed to init SDL");

	window = SDL_CreateWindow(title.data(), width, height, windowFlags);
	if (!window)
		throw SDLError("failed to create window");

	log(SDL_LOG_PRIORITY_INFO, "loading GPU device and pipeline...");
	loadDevice();
	loadPipeline();
	particleSet.init(gpuDevice);
	loadComputePipeline();

	SDL_ShowWindow(window);
	toggleMouseGrab();
	updateProjMatrix(width, height);
	keyboard.fill(false);
}

GPUNewtonApp::TimePoint GPUNewtonApp::currentTime()
{
	return std::chrono::steady_clock::now();
}

void GPUNewtonApp::debugDelta()
{
#if !defined(NDEBUG) && PRINT_FPS
	static TimePoint last;

	const auto fps = std::chrono::seconds(1) / delta;
	if ((currentTime() - last) >= std::chrono::seconds(1)) {
		std::cout << "Delta time: " << delta << ", FPS: " << fps << std::endl;
		last = currentTime();
	}
#endif
}

void GPUNewtonApp::toggleMouseGrab()
{
	static bool enabled;
	enabled = !enabled;
	SDL_SetWindowRelativeMouseMode(window, enabled);
}

bool GPUNewtonApp::isWindowFocused()
{
	const SDL_WindowFlags flags = SDL_GetWindowFlags(window);
	return !!(flags & SDL_WINDOW_INPUT_FOCUS);
}

void GPUNewtonApp::handleEvents()
{
	SDL_Event event;

	while (SDL_PollEvent(&event)) {
		switch (event.type) {
			case SDL_EVENT_QUIT:
				running = false;
				break;
			case SDL_EVENT_WINDOW_RESIZED:
				updateProjMatrix(event.window.data1, event.window.data2);
				break;
			case SDL_EVENT_MOUSE_MOTION:
				updateCameraMouse(event.motion.xrel, event.motion.yrel);
				break;
			case SDL_EVENT_KEY_DOWN:
				keyboard[event.key.scancode] = true;
				break;
			case SDL_EVENT_KEY_UP:
				keyboard[event.key.scancode] = false;
				break;
		}
	}

	if (keyPressed(SDLK_ESCAPE, true))
		toggleMouseGrab();
}

void GPUNewtonApp::loop()
{
	running = true;
	TimePoint lastTime = currentTime();

	particleSet.upload();
	while (running) {
		handleEvents();

		SDL_GPUBuffer * const particleSetBuffer = particleSet.getBuffer();
		const auto particleSetNum = particleSet.getNum();

		cmdBuffer = SDL_AcquireGPUCommandBuffer(gpuDevice);
		if (!cmdBuffer) {
			log(SDL_LOG_PRIORITY_ERROR, "failed to acquire command buffer: %s", SDL_GetError());
			continue;
		}

		TimePoint thisTime = currentTime();
		delta = thisTime - lastTime;
		debugDelta();

		/* Simulation code here */
		{
			updateCameraPos(keyboard);

			SDL_GPUStorageBufferReadWriteBinding bufferBinding{};
			bufferBinding.buffer = particleSet.getBuffer();
			bufferBinding.cycle = false;
			SDL_GPUComputePass *computePass = SDL_BeginGPUComputePass(cmdBuffer, nullptr, 0, &bufferBinding, 1);
			if (!computePass) {
				log(SDL_LOG_PRIORITY_ERROR, "failed to acquire compute pass: %s", SDL_GetError());
				continue;
			}
			SDL_BindGPUComputePipeline(computePass, compPipeline);
			SDL_BindGPUComputeStorageBuffers(computePass, 0, &particleSetBuffer, 1);
			SDL_DispatchGPUCompute(computePass, 1, 1, NPARTICLES);
			SDL_EndGPUComputePass(computePass);
		}
		/* End of simulation code */

		lastTime = thisTime;

		SDL_GPUTexture *swapTexture;
		if (!SDL_WaitAndAcquireGPUSwapchainTexture(cmdBuffer, window, &swapTexture, nullptr, nullptr))
			continue;

		if (swapTexture && isWindowFocused()) {
			glm::mat4 combined;
			createCombined(combined);

			SDL_GPUColorTargetInfo colorTargetInfo{};
			colorTargetInfo.texture = swapTexture;
			colorTargetInfo.clear_color = SDL_FColor(0.1f, 0.0f, 0.2f, 1.0f);
			colorTargetInfo.load_op = SDL_GPU_LOADOP_CLEAR;
			colorTargetInfo.store_op = SDL_GPU_STOREOP_STORE;

			SDL_GPURenderPass *renderPass = SDL_BeginGPURenderPass(cmdBuffer, &colorTargetInfo, 1, nullptr);
			if (!renderPass) {
				log(SDL_LOG_PRIORITY_ERROR, "failed to acquire render pass: %s", SDL_GetError());
				continue;
			}

			SDL_BindGPUGraphicsPipeline(renderPass, gpuPipeline);
			SDL_BindGPUVertexStorageBuffers(renderPass, 0, &particleSetBuffer, 1);

			CamInfo camInfo { .combined = combined };

			SDL_PushGPUVertexUniformData(cmdBuffer, 0, &camInfo, sizeof(CamInfo));
			SDL_DrawGPUPrimitives(renderPass, 3, particleSetNum, 0, 0);
			SDL_EndGPURenderPass(renderPass);
		}

		SDL_SubmitGPUCommandBuffer(cmdBuffer);
	}
}

GPUNewtonApp::~GPUNewtonApp()
{
	particleSet.deinit();

	if (gpuPipeline)
		SDL_ReleaseGPUGraphicsPipeline(gpuDevice, gpuPipeline);

	if (gpuDevice)
		SDL_DestroyGPUDevice(gpuDevice);

	if (window)
		SDL_DestroyWindow(window);

	SDL_Quit();
}

int main()
{
	try {
		GPUNewtonApp app("GPU Newton", 800, 500);
		app.loop();
	} catch (std::exception &e) {
		log(SDL_LOG_PRIORITY_CRITICAL, "%s", e.what());
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
