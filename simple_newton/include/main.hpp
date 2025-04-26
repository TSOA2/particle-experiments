#ifndef _SIMPLE_NEWTON_MAIN_HEADER_FILE
#define _SIMPLE_NEWTON_MAIN_HEADER_FILE

#include <SDL3/SDL.h>
#include <glm/vec2.hpp>
#include <glm/geometric.hpp>
#include <BS_thread_pool.hpp>

#include <iostream>
#include <random>
#include <thread>
#include <atomic>
#include <sstream>
#include <functional>

/*
 * The number of particles in the simulation.
 */
#define NUM_PARTICLES (2000)

/*
 * The lowest possible particle mass.
 */
#define MASS_LOW (1e2)

/*
 * The highest possible particle mass.
 */
#define MASS_HIGH (1e9)

/*
 * Determines whether the particles collide with the walls.
 */
#define WALL_COLLISION (true)

/*
 * How much of their original velocity to the particles have after
 * bouncing off a wall.
 */
#define WALL_ABSORB (0.1)

/*
 * Increase this to increase the timestep of the simulation (this
 * will decrease the precision)
 */
#define TIMESTEP (1)

struct SDLError {
	std::string_view msg;
	template <class T>
	constexpr SDLError(T msg):
		msg(msg) {}

	virtual const char *what() const noexcept {
		std::stringstream s;
		s << msg << " | " << SDL_GetError();
		return s.view().data();
	}
};

class ParticleSet {
	private:
		struct ParticleInfo {
			glm::dvec2 veloc;
			double mass;
		};

		/* Positions stored here */
		std::vector<SDL_FPoint> points;
		std::vector<ParticleInfo> infos;

	public:
		ParticleSet(std::size_t nParticles, int width, int height);
		void updateParticles(double delta, int width, int height);
		void draw(SDL_Renderer *render);
};

class SimpleNewtonApp {
	private:
		SDL_Window *window;
		int width;
		int height;

		SDL_Renderer *render;
		bool running;

		void handleEvents();

	public:
		SimpleNewtonApp(const SimpleNewtonApp &) = delete;
		SimpleNewtonApp(SimpleNewtonApp &&) = delete;

		SimpleNewtonApp(std::string_view title, int width, int height);

		void loop();

		~SimpleNewtonApp();
};

#endif // _SIMPLE_NEWTON_MAIN_HEADER_FILE
