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
		points.emplace_back( SDL_FPoint { wdisr(rng), hdisr(rng) });

		infos.emplace_back(ParticleInfo {
			glm::vec2(0, 0),
			mdisr(rng)
		});
	}
}

void ParticleSet::updateParticles(double delta, [[maybe_unused]] int width, [[maybe_unused]] int height)
{
	for (std::size_t i = 0; i < points.size(); i++) {
		threadPool.detach_task([&, i]() {
			glm::dvec2 accelVector = glm::dvec2(0, 0);
			for (std::size_t j = 0; j < points.size(); j++) {
				if (i == j)
					continue;

				const glm::dvec2 dVector =
					glm::dvec2(points[j].x, points[j].y) -
					glm::dvec2(points[i].x, points[i].y);

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
				double dMagn     = glm::dot(dVector, dVector);
				if (dMagn == 0)
					dMagn = 1;
				const double aScalar   = (infos[j].mass * gConstant) / dMagn;
				accelVector += dVector * aScalar;
			}
			infos[i].veloc += accelVector;
			glm::dvec2 finalVector = infos[i].veloc * delta;
			points[i].x += finalVector.x;
			points[i].y += finalVector.y;
#if WALL_COLLISION
			const float wfloat = static_cast<float>(width);
			const float hfloat = static_cast<float>(height);
			if (points[i].x >= wfloat || points[i].x <= 0) {
				points[i].x = glm::min(glm::max(points[i].x, 0.0f), wfloat);
				infos[i].veloc.x *= -WALL_ABSORB;
			}

			if (points[i].y >= hfloat || points[i].y <= 0) {
				points[i].y = glm::min(glm::max(points[i].y, 0.0f), hfloat);
				infos[i].veloc.y *= -WALL_ABSORB;
			}
#endif
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

		particleSet.updateParticles(TIMESTEP, width, height);
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
