#include <main.hpp>

BS::thread_pool threadPool;

ParticleSet::ParticleSet(std::size_t nParticles, int width, int height)
{
	std::random_device device;
	std::mt19937 rng(device());

	const float wfloat = static_cast<float>(width);
	const float hfloat = static_cast<float>(height);
	std::uniform_real_distribution<float> wdisr(wfloat/3, 2*wfloat/3);
	std::uniform_real_distribution<float> hdisr(hfloat/3, 2*hfloat/3);
	std::uniform_real_distribution<float> mdisr(MASS_LOW, MASS_HIGH);

	points.reserve(nParticles);
	infos.reserve(nParticles);
	for (std::size_t i = 0; i < nParticles; i++) {
		const float x = wdisr(rng);
		const float y = hdisr(rng);
		points.emplace_back(SDL_FPoint { x, y });
		infos.emplace_back(ParticleInfo {
			glm::vec2(x, y),
			glm::vec2(0, 0),
			mdisr(rng)
		});
	}
}

void ParticleSet::updateParticles(const ParticleSet::UpdateInfo &updateInfo)
{
	for (std::size_t i = 0; i < points.size(); i++) {
		threadPool.detach_task([&, i]() {
			glm::dvec2 accelVector = glm::dvec2(0, 0);
			for (std::size_t j = 0; j < points.size(); j++) {
				if (i == j)
					continue;

				const glm::dvec2 dVector = infos[j].pos - infos[i].pos;
				const double gConstant = 6.6743e-11;

				/*
				 * This is a 2D simulation, so the formula is really
				 * (G * m1) / R
				 * Instead of
				 * (G * m1) / R^2
				 *
				 * However, because we need to normalize dVector anyways,
				 * we have to divide twice by the radius, effectively
				 * divided by R^2. That's why dMagn is R^2 and not R
				 */
				double dMagn = glm::dot(dVector, dVector);
				if (dMagn == 0)
					dMagn = 1;
				const double aScalar = (infos[j].mass * gConstant) / dMagn;
				accelVector += dVector * aScalar;
			}
			infos[i].veloc += accelVector;
			glm::dvec2 finalVector = infos[i].veloc * updateInfo.delta;
			infos[i].pos += finalVector;
#if WALL_COLLISION
			const float wfloat = static_cast<float>(updateInfo.width);
			const float hfloat = static_cast<float>(updateInfo.height);
			if (infos[i].pos.x >= wfloat || infos[i].pos.x <= 0) {
				infos[i].pos.x = glm::min(glm::max(infos[i].pos.x, 0.0f), wfloat);
				infos[i].veloc.x *= -WALL_ABSORB;
			}

			if (infos[i].pos.y >= hfloat || infos[i].pos.y <= 0) {
				infos[i].pos.y = glm::min(glm::max(infos[i].pos.y, 0.0f), hfloat);
				infos[i].veloc.y *= -WALL_ABSORB;
			}
#endif

			points[i].x = static_cast<float>(updateInfo.camScale * (infos[i].pos.x + updateInfo.camPos.x));
			points[i].y = static_cast<float>(updateInfo.camScale * (infos[i].pos.y + updateInfo.camPos.y));
		});
	}

	threadPool.wait();
}

void ParticleSet::draw(SDL_Renderer *render)
{
	const std::uint8_t red   = 255;
	const std::uint8_t green = 0;
	const std::uint8_t blue  = 100;
	(void) SDL_SetRenderDrawColor(render, red, green, blue, 0);
	(void) SDL_RenderPoints(render, points.data(), static_cast<int>(points.size()));
}

SimpleNewtonApp::SimpleNewtonApp(std::string_view title, int width, int height):
	window(nullptr), width(width), height(height), render(nullptr), running(false)
{
	const SDL_InitFlags initFlags = SDL_INIT_VIDEO;
	const SDL_WindowFlags windowFlags = SDL_WINDOW_RESIZABLE;

	if (!SDL_Init(initFlags))
		throw SDLError("failed to init SDL3");

	window = SDL_CreateWindow(title.data(), width, height, windowFlags);
	if (!window)
		throw SDLError("failed to create window");

	render = SDL_CreateRenderer(window, nullptr);
	if (!render)
		throw SDLError("failed to create renderer");
}

void SimpleNewtonApp::calcScale(const SDL_Event &event)
{
	float y = event.wheel.y;
	if (event.wheel.direction == SDL_MOUSEWHEEL_FLIPPED)
		y *= -1;

	camScale += y * 0.001;
	if (camScale <= 0)
		camScale = 0;
}

void SimpleNewtonApp::calcMove(const SDL_Event &event)
{
	if (event.motion.state & SDL_BUTTON_LMASK) {
		camPos.x += event.motion.xrel * 100;
		camPos.y += event.motion.yrel * 100;
	}
}

void SimpleNewtonApp::handleEvents()
{
	SDL_Event event;
	while (SDL_PollEvent(&event)) {
		switch (event.type) {
			case SDL_EVENT_QUIT:
				running = false;
				break;
			case SDL_EVENT_WINDOW_RESIZED:
				width = event.window.data1;
				height = event.window.data2;
				break;
			case SDL_EVENT_MOUSE_WHEEL:
				calcScale(event);
				break;
			case SDL_EVENT_MOUSE_MOTION:
				calcMove(event);
				break;
		}
	}
}

void SimpleNewtonApp::loop()
{
	ParticleSet particleSet(NUM_PARTICLES, width, height);

	running = true;
	while (running) {
		handleEvents();

		(void) SDL_SetRenderDrawColor(render, 10, 0, 20, 0);
		(void) SDL_RenderClear(render);

		particleSet.draw(render);

		(void) SDL_RenderPresent(render);

		ParticleSet::UpdateInfo info{};
		info.camPos = camPos;
		info.camScale = camScale;
		info.delta = TIMESTEP;
		info.width = width;
		info.height = height;

		particleSet.updateParticles(info);
	}
}

SimpleNewtonApp::~SimpleNewtonApp()
{
	if (render)
		SDL_DestroyRenderer(render);

	if (window)
		SDL_DestroyWindow(window);

	SDL_Quit();
}

int main()
{
	SimpleNewtonApp app("Simple Newton", 700, 500);
	app.loop();
}
