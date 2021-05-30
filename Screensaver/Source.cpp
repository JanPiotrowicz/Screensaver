/*	Screensaver based on Slime-Simulation by Sebastian League: https://github.com/SebLague/Slime-Simulation/
*	Limiting factor in performance is number of dots as their logic is handled
*	by CPU. Blur is written as shader so resolution (# of pixels) is not an issue.
*/	

#include <SFML/Graphics.hpp>
#include <iostream>
#include <random>

#define Use_CPU_Blur 0	// Set to 0 to use fast GPU blur shader instead of slow CPU function
#define Spawn_Mode 0	// 0 - Random, 1 - Center, 2 - Circle

// All the variables to play with
float width = 1920.0f, height = 1080.0f;
unsigned int numberOfAgents = 50'000;
const float PI = 3.1415927f;
const float speed = 1.5f;
const float changeDirAngle = 0.01745329f * 12.0f;	// one deg * angle
const int sensorSize = 1;	// size of sampled area
const float blurOffset = 1.01f;

sf::Image TrailsMap;

std::string BlurShaderString = "\
#ifdef GL_ES\n\
precision mediump float;\n\
#endif\n\
\n\
uniform sampler2D MyTexture;\n\
uniform vec2 textureSize;\n\
uniform float offset;\n\
\n\
void main()\n\
{\n\
	vec2 onePixel = vec2(1.0f, 1.0f) / textureSize;\n\
	vec2 MyCoord = gl_TexCoord[0].xy;\n\
\n\
	vec4 color;\n\
	color.rgba = vec4(0.0f);\n\
\n\
	int pixelCount = 0;\n\
\n\
	// blur image\n\
	if (offset < 1.2)\n\
	{\n\
		gl_FragColor = (\n\
			0.5 * texture2D(MyTexture, MyCoord + vec2(-1.0f, -1.0f) * onePixel) +\n\
			1.5 * texture2D(MyTexture, MyCoord + vec2(-1.0f, +0.0f) * onePixel) +\n\
			0.5 * texture2D(MyTexture, MyCoord + vec2(-1.0f, +1.0f) * onePixel) +\n\
\n\
			1.5 * texture2D(MyTexture, MyCoord + vec2(+0.0f, -1.0f) * onePixel) +\n\
			4.0 * texture2D(MyTexture, MyCoord + vec2(+0.0f, +0.0f) * onePixel) +\n\
			1.5 * texture2D(MyTexture, MyCoord + vec2(+0.0f, +1.0f) * onePixel) +\n\
\n\
			0.5 * texture2D(MyTexture, MyCoord + vec2(+1.0f, -1.0f) * onePixel) +\n\
			1.5 * texture2D(MyTexture, MyCoord + vec2(+1.0f, +0.0f) * onePixel) +\n\
			0.5 * texture2D(MyTexture, MyCoord + vec2(+1.0f, +1.0f) * onePixel)) / 12.0f;\n\
	}\n\
	else\n\
	{\n\
		for (float offsetX = -offset; offsetX < offset; offsetX += 1.0f)\n\
		{\n\
			for (float offsetY = -offset; offsetY < offset; offsetY += 1.0f)\n\
			{\n\
				color += texture2D(MyTexture, MyCoord + vec2(offsetX, offsetY) * onePixel);\n\
				pixelCount++;\n\
			}\n\
		}\n\
\n\
		gl_FragColor = color / max(1, pixelCount);\n\
	}\n\
\n\
	// degrade color\n\
	gl_FragColor.r = max(0.0f, gl_FragColor.r - 0.04f);\n\
	gl_FragColor.g = max(0.0f, gl_FragColor.g - 0.03f);\n\
	gl_FragColor.b = max(0.0f, gl_FragColor.b - 0.01f);\n\
}";	// Shader in String form

float Random01()	// random number from 0 to 1
{
	unsigned int ret = rand();
	ret ^= (ret << 17);
	ret *= 292549106;
	ret ^= (ret >> 13);
	ret ^= (ret >> 5);

	return static_cast<float>((ret % 1024) / 1024.0f);
}

class Agent
{
public:
#if Spawn_Mode == 0
	Agent()
		: m_Position(Random01()* width, Random01()* height), m_Angle(Random01() * 2 * PI) {}
#elif Spawn_Mode == 1
	Agent()
		: m_Position(width / 2, height / 2), m_Angle(Random01() * 2 * PI) {}
#else
	Agent(float radius = 250)
		: m_Position(0.1f, 0.1f), m_Angle(Random01() * 2 * PI)
	{
		while ((m_Position.x - width / 2) * (m_Position.x - width / 2) + (m_Position.y - height / 2) * (m_Position.y - height / 2) > radius * radius)
		{
			m_Position.x = width / 2 + 2 * radius * (0.5f - Random01());
			m_Position.y = height / 2 + 2 * radius * (0.5f - Random01());
		}
	}
#endif

	Agent(float radius, sf::Vector2f center)
		: m_Position(0.1f, 0.1f), m_Angle(Random01() * 2 * PI)
	{
		while ((m_Position.x - center.x) * (m_Position.x - center.x) + (m_Position.y - center.y) * (m_Position.y - center.y) > radius * radius)
		{
			m_Position.x = center.x + 2 * radius * (0.5f - Random01());
			m_Position.y = center.y + 2 * radius * (0.5f - Random01());
		}
	}

	Agent(sf::Vector2f pos)
		: m_Position(pos), m_Angle(Random01() * 2 * PI) {}

public:
	sf::Vector2f m_Position;
	float m_Angle;
};

float sample_area(sf::Vector2f point)
{
	float sum = 0;

	for (int offsetX = static_cast<int>(point.x - sensorSize); offsetX <= point.x + sensorSize; offsetX++)
	{
		for (int offsetY = static_cast<int>(point.y - sensorSize); offsetY <= point.y + sensorSize; offsetY++)
		{
			if (offsetX >= 0 && offsetX < width && offsetY >= 0 && offsetY < height)
			{
				sf::Color color = TrailsMap.getPixel(offsetX, offsetY);
				sum += color.r + color.g + color.b;
			}
		}
	}

	return sum;
}

void UpadteAgent(Agent& agent)
{
	sf::Vector2f LeftPoint;
	LeftPoint.x = agent.m_Position.x + 5 * cos(agent.m_Angle - 2 * changeDirAngle) * speed;
	LeftPoint.y = agent.m_Position.y + 5 * sin(agent.m_Angle - 2 * changeDirAngle) * speed;

	sf::Vector2f RightPoint;
	RightPoint.x = agent.m_Position.x + 5 * cos(agent.m_Angle + 2 * changeDirAngle) * speed;
	RightPoint.y = agent.m_Position.y + 5 * sin(agent.m_Angle + 2 * changeDirAngle) * speed;

	sf::Vector2f MiddlePoint;
	MiddlePoint.x = agent.m_Position.x + 5 * cos(agent.m_Angle) * speed;
	MiddlePoint.y = agent.m_Position.y + 5 * sin(agent.m_Angle) * speed;

	float senseLeft = sample_area(LeftPoint);
	float senseMiddle = sample_area(MiddlePoint);
	float senseRight = sample_area(RightPoint);

	sf::Vector2f newPosition;

	if (senseMiddle > senseLeft && senseMiddle > senseRight)
	{
		agent.m_Angle += changeDirAngle * 0.5f * (Random01() - 0.5f);
	}
	else if (senseMiddle < senseLeft && senseMiddle < senseRight)
	{
		agent.m_Angle += changeDirAngle * (Random01() - 0.5f);
	}
	else if (senseRight > senseLeft)
	{
		agent.m_Angle += changeDirAngle * Random01();
	}
	else if (senseLeft > senseRight)
	{
		agent.m_Angle -= changeDirAngle * Random01();
	}

	newPosition.x = agent.m_Position.x + cos(agent.m_Angle) * speed;
	newPosition.y = agent.m_Position.y + sin(agent.m_Angle) * speed;

	if (newPosition.x < 0 || newPosition.x >= width || newPosition.y < 0 || newPosition.y >= height)
	{
		newPosition = { Random01() * width, Random01() * height };
		agent.m_Angle = (Random01() * 2 * PI);
	}

	agent.m_Position = newPosition;
	TrailsMap.setPixel(static_cast<unsigned int>(newPosition.x), static_cast<unsigned int>(newPosition.y), sf::Color::White);
}

#if Use_CPU_Blur
void DiffuseMap()
{
	sf::Image TrailsMapCopy = TrailsMap;
	for (int i = 0; i < width; i++)
	{
		for (int j = 0; j < height; j++)
		{
			if (i <= 0 || j <= 0 || i >= width - 1 || j >= height - 1)
			{
				TrailsMap.setPixel(i, j, sf::Color::Black);
				continue;
			}

			sf::Color color;

			sf::Color col1 = TrailsMap.getPixel(i - 1, j - 1);
			sf::Color col2 = TrailsMap.getPixel(i - 1, j);
			sf::Color col3 = TrailsMap.getPixel(i - 1, j + 1);

			sf::Color col4 = TrailsMap.getPixel(i, j - 1);
			sf::Color col6 = TrailsMap.getPixel(i, j + 1);

			sf::Color col7 = TrailsMap.getPixel(i + 1, j - 1);
			sf::Color col8 = TrailsMap.getPixel(i + 1, j);
			sf::Color col9 = TrailsMap.getPixel(i + 1, j + 1);

			color.r = ((sf::Uint16)col1.r + (sf::Uint16)col2.r + (sf::Uint16)col3.r + (sf::Uint16)col4.r
				+ (sf::Uint16)col6.r + (sf::Uint16)col7.r + (sf::Uint16)col8.r + (sf::Uint16)col9.r) / 9;
			color.g = ((sf::Uint16)col1.g + (sf::Uint16)col2.g + (sf::Uint16)col3.g + (sf::Uint16)col4.g
				+ (sf::Uint16)col6.g + (sf::Uint16)col7.g + (sf::Uint16)col8.g + (sf::Uint16)col9.g) / 9;
			color.b = ((sf::Uint16)col1.b + (sf::Uint16)col2.b + (sf::Uint16)col3.b + (sf::Uint16)col4.b
				+ (sf::Uint16)col6.b + (sf::Uint16)col7.b + (sf::Uint16)col8.b + (sf::Uint16)col9.b) / 9;

			sf::Color origCol = TrailsMap.getPixel(i, j);
			color.r = ((sf::Uint16)2 * origCol.r + 1 * (sf::Uint16)color.r) / 3;
			color.g = ((sf::Uint16)2 * origCol.g + 1 * (sf::Uint16)color.g) / 3;
			color.b = ((sf::Uint16)2 * origCol.b + 1 * (sf::Uint16)color.b) / 3;

			color.r *= 0.99f;
			color.g *= 0.99f;
			color.b *= 0.99f;

			TrailsMapCopy.setPixel(i, j, color);
		}
	}
	TrailsMap = TrailsMapCopy;
}
#endif // Use_CPU_Blur

int main()
{
	srand(static_cast<unsigned int>(time(NULL)));

#if _ITERATOR_DEBUG_LEVEL > 0
	numberOfAgents = 5'000;
	auto Style = sf::Style::Default;
#else
	auto Style = sf::Style::Fullscreen;
#endif // DEBUG_LEVEL > 0

	width = static_cast<float>(sf::VideoMode::getDesktopMode().width);
	height = static_cast<float>(sf::VideoMode::getDesktopMode().height);

	sf::RenderWindow window(sf::VideoMode::getDesktopMode(), "Screensaver", Style);
	window.setMouseCursorVisible(false);
	window.setFramerateLimit(144);

	TrailsMap.create(static_cast<unsigned int>(width), static_cast<unsigned int>(height), sf::Color::Black);
	sf::Texture texture;
	texture.loadFromImage(TrailsMap);

	sf::RenderTexture RendTargForShader;
	RendTargForShader.create(static_cast<unsigned int>(width), static_cast<unsigned int>(height));

	sf::Sprite sprite(texture);

	sf::Shader BlurShader;
	BlurShader.loadFromMemory(BlurShaderString, sf::Shader::Fragment);
	//BlurShader.loadFromFile("blur.glsl", sf::Shader::Fragment);

	BlurShader.setUniform("textureSize", sf::Vector2f(width, height));
	BlurShader.setUniform("MyTexture", texture);
	BlurShader.setUniform("offset", blurOffset);

	Agent* AgentsTable = new Agent[numberOfAgents];

	while (window.isOpen())
	{
		for (unsigned int i = 0; i < numberOfAgents; i++)
			UpadteAgent(AgentsTable[i]);

		texture.loadFromImage(TrailsMap);
		sprite.setTexture(texture);

#if Use_CPU_Blur
		DiffuseMap();
#else
		RendTargForShader.clear(sf::Color::Black);

		RendTargForShader.draw(sprite, &BlurShader);

		RendTargForShader.display();

		TrailsMap = RendTargForShader.getTexture().copyToImage();

		sprite.setTexture(RendTargForShader.getTexture());
#endif

		sf::Event event;
		while (window.pollEvent(event))
		{
			switch (event.type)
			{
			case sf::Event::Closed:
				window.close();
				break;

			/*case sf::Event::KeyPressed:
				window.close();
				break;*/

			case sf::Event::MouseButtonPressed:
				window.close();
				break;

			default:
				break;
			}
		}

		window.clear(sf::Color::Black);

		window.draw(sprite);

		window.display();
	}

	return 0;
}
