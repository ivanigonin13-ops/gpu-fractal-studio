#include <SFML/Graphics.hpp>
#include <iostream>
#include <cstdlib> 
#include <ctime>   
#include <cmath>

struct Camera {
    float centerX = 0.0f;
    float centerY = 0.0f;
    float zoom = 1.0f;
};

// Вшитый код фрагментного шейдера (GLSL #version 110)
const std::string shaderSource = R"(#version 110
uniform vec2 u_resolution;
uniform vec2 u_camera_center;
uniform float u_zoom;
uniform int u_max_iter;
uniform bool u_is_mandelbrot;
uniform vec2 u_julia_c;

uniform float u_r_phase;
uniform float u_g_phase;
uniform float u_b_phase;
uniform float u_frequency;

void main() {
    float base_width_re = 3.0;
    float base_height_im = 2.4;
    
    float aspect = u_resolution.x / u_resolution.y;
    float width_re = (base_width_re / u_zoom) * (aspect / 1.333);
    float height_im = base_height_im / u_zoom;
    
    float min_re = u_camera_center.x - width_re / 2.0;
    float max_re = u_camera_center.x + width_re / 2.0;
    float min_im = u_camera_center.y - height_im / 2.0;
    float max_im = u_camera_center.y + height_im / 2.0;
    
    float zr = min_re + gl_FragCoord.x * (max_re - min_re) / u_resolution.x;
    float zi = min_im + (u_resolution.y - gl_FragCoord.y) * (max_im - min_im) / u_resolution.y;
    
    vec2 z;
    vec2 c;
    
    if (u_is_mandelbrot) {
        z = vec2(0.0, 0.0);
        c = vec2(zr, zi);
    } else {
        z = vec2(zr, zi);
        c = u_julia_c;
    }
    
    int iter = 0;
    float nrm = 0.0;
    
    // ИСПРАВЛЕНО: Строгий цикл for. Теперь видеокарта гарантированно
    // слушается переменную u_max_iter из C++ кода.
    for (int i = 0; i < 2000; i++) {
        if (i >= u_max_iter) {
            break;
        }
        
        nrm = z.x * z.x + z.y * z.y;
        if (nrm > 16.0) {
            break;
        }
        
        float next_r = z.x * z.x - z.y * z.y + c.x;
        float next_i = 2.0 * z.x * z.y + c.y;
        z = vec2(next_r, next_i);
        iter++;
    }
    
    if (iter == u_max_iter) {
        gl_FragColor = vec4(0.0, 0.0, 0.0, 1.0);
    } else {
        float log_zn = log(nrm) / 2.0;
        float nu = log(log_zn / log(2.0)) / log(2.0);
        float iter_smooth = float(iter) + 1.0 - nu;
        
        float r = sin(u_frequency * iter_smooth + u_r_phase) * 0.5 + 0.5;
        float g = sin(u_frequency * iter_smooth + u_g_phase) * 0.5 + 0.5;
        float b = sin(u_frequency * iter_smooth + u_b_phase) * 0.5 + 0.5;
        
        gl_FragColor = vec4(r, g, b, 1.0);
    }
}
)";

int main() {
    std::srand(static_cast<unsigned int>(std::time(nullptr)));

    const int WIDTH = 960;
    const int HEIGHT = 720;

    sf::RenderWindow window(sf::VideoMode(WIDTH, HEIGHT), "Fixed Quality Fractal GPU");
    window.setFramerateLimit(60);

    if (!sf::Shader::isAvailable()) {
        std::cerr << "Критическая ошибка: Шейдеры не поддерживаются видеокартой!" << std::endl;
        return -1;
    }

    sf::Shader shader;
    if (!shader.loadFromMemory(shaderSource, sf::Shader::Fragment)) return -1;

    sf::RectangleShape surface(sf::Vector2f(static_cast<float>(WIDTH), static_cast<float>(HEIGHT)));

    Camera cam;
    int maxIter = 80; // Начальное качество (сделайте поменьше, чтобы видеть изменения)
    bool isMandelbrot = true;

    sf::Vector2f juliaC(-0.7f, 0.27015f);

    float frequency = 0.15f;
    float rPhase = static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX) * 6.2831f;
    float gPhase = static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX) * 6.2831f;
    float bPhase = static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX) * 6.2831f;

    sf::Clock dtClock; 

    while (window.isOpen()) {
        float deltaTime = dtClock.restart().asSeconds();
        sf::Vector2u size = window.getSize();

        sf::Event event;
        while (window.pollEvent(event)) {
            if (event.type == sf::Event::Closed) {
                window.close();
            }
            else if (event.type == sf::Event::Resized) {
                sf::FloatRect visibleArea(0.f, 0.f, static_cast<float>(event.size.width), static_cast<float>(event.size.height));
                window.setView(sf::View(visibleArea));
                surface.setSize(sf::Vector2f(static_cast<float>(event.size.width), static_cast<float>(event.size.height)));
            }
            else if (event.type == sf::Event::MouseWheelScrolled) {
                if (event.mouseWheelScroll.wheel == sf::Mouse::VerticalWheel) {
                    float mouseNormX = (static_cast<float>(event.mouseWheelScroll.x) / size.x) - 0.5f;
                    float mouseNormY = (static_cast<float>(event.mouseWheelScroll.y) / size.y) - 0.5f;
                    
                    float aspect = static_cast<float>(size.x) / size.y;
                    float currentWidthRe = (3.0f / cam.zoom) * (aspect / 1.333f);
                    float currentHeightIm = 2.4f / cam.zoom;

                    float mouseRe = cam.centerX + mouseNormX * currentWidthRe;
                    float mouseIm = cam.centerY + mouseNormY * currentHeightIm;

                    if (event.mouseWheelScroll.delta > 0) {
                        cam.zoom *= 1.25f;
                    } else {
                        cam.zoom /= 1.25f;
                    }

                    float newWidthRe = (3.0f / cam.zoom) * (aspect / 1.333f);
                    float newHeightIm = 2.4f / cam.zoom;
                    cam.centerX = mouseRe - mouseNormX * newWidthRe;
                    cam.centerY = mouseIm - mouseNormY * newHeightIm;
                }
            }
            else if (event.type == sf::Event::KeyPressed) {
                if (event.key.code == sf::Keyboard::Space) { isMandelbrot = !isMandelbrot; }
                
                // ИЗМЕНЕНИЕ КАЧЕСТВА НА СТРЕЛОЧКАХ (Шаг увеличен до 25 для наглядности)
                if (event.key.code == sf::Keyboard::Up) { 
                    maxIter += 25; 
                    if (maxIter > 2000) maxIter = 2000; // Ограничение сверху под цикл шейдера
                    std::cout << "Текущее качество (итерации): " << maxIter << std::endl;
                }
                if (event.key.code == sf::Keyboard::Down) { 
                    if (maxIter > 10) maxIter -= 25; 
                    if (maxIter < 10) maxIter = 10; // Минимальное качество
                    std::cout << "Текущее качество (итерации): " << maxIter << std::endl;
                }
                
                if (event.key.code == sf::Keyboard::C) {
                    rPhase = static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX) * 6.2831f;
                    gPhase = static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX) * 6.2831f;
                    bPhase = static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX) * 6.2831f;
                }

                if (event.key.code == sf::Keyboard::R) {
                    float randX = (static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX)) * 2.0f - 1.0f;
                    float randY = (static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX)) * 1.2f - 0.6f;
                    juliaC = sf::Vector2f(randX, randY);
                    std::cout << "Новая константа Жюлиа: " << juliaC.x << " + " << juliaC.y << "i" << std::endl;
                }
            }
        }

        float baseSpeed = 1.2f; 
        float currentSpeed = (baseSpeed / cam.zoom) * deltaTime;

        if (sf::Keyboard::isKeyPressed(sf::Keyboard::A)) cam.centerX -= currentSpeed;
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::D)) cam.centerX += currentSpeed;
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::W)) cam.centerY -= currentSpeed;
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::S)) cam.centerY += currentSpeed;

        shader.setUniform("u_resolution", sf::Vector2f(static_cast<float>(size.x), static_cast<float>(size.y)));
        shader.setUniform("u_camera_center", sf::Vector2f(cam.centerX, cam.centerY));
        shader.setUniform("u_zoom", cam.zoom);
        shader.setUniform("u_max_iter", maxIter);
        shader.setUniform("u_is_mandelbrot", isMandelbrot);
        shader.setUniform("u_julia_c", juliaC); 
        
        shader.setUniform("u_frequency", frequency);
        shader.setUniform("u_r_phase", rPhase);
        shader.setUniform("u_g_phase", gPhase);
        shader.setUniform("u_b_phase", bPhase);

        window.clear();
        window.draw(surface, &shader);
        window.display();
    }

    return 0;
}
