#ifdef GL_ES
precision mediump float;
#endif

uniform sampler2D MyTexture;
uniform vec2 textureSize;
uniform float offset;

void main()
{
	vec2 onePixel = vec2(1.0f, 1.0f) / textureSize;
	vec2 MyCoord = gl_TexCoord[0].xy;

	vec4 color;
	color.rgba = vec4(0.0f);

	int pixelCount = 0;

	// blur image
	if (offset < 1.2)
	{
		gl_FragColor = (
			0.5 * texture2D(MyTexture, MyCoord + vec2(-1.0f, -1.0f) * onePixel) +
			1.5 * texture2D(MyTexture, MyCoord + vec2(-1.0f, +0.0f) * onePixel) +
			0.5 * texture2D(MyTexture, MyCoord + vec2(-1.0f, +1.0f) * onePixel) +

			1.5 * texture2D(MyTexture, MyCoord + vec2(+0.0f, -1.0f) * onePixel) +
			4.0 * texture2D(MyTexture, MyCoord + vec2(+0.0f, +0.0f) * onePixel) +
			1.5 * texture2D(MyTexture, MyCoord + vec2(+0.0f, +1.0f) * onePixel) +

			0.5 * texture2D(MyTexture, MyCoord + vec2(+1.0f, -1.0f) * onePixel) +
			1.5 * texture2D(MyTexture, MyCoord + vec2(+1.0f, +0.0f) * onePixel) +
			0.5 * texture2D(MyTexture, MyCoord + vec2(+1.0f, +1.0f) * onePixel)) / 12.0f;
	}
	else
	{
		for (float offsetX = -offset; offsetX < offset; offsetX += 1.0f)
		{
			for (float offsetY = -offset; offsetY < offset; offsetY += 1.0f)
			{
				color += texture2D(MyTexture, MyCoord + vec2(offsetX, offsetY) * onePixel);
				pixelCount++;
			}
		}

		gl_FragColor = color / max(1, pixelCount);
	}

	// degrade color
	gl_FragColor.r = max(0.0f, gl_FragColor.r - 0.05f);
	gl_FragColor.g = max(0.0f, gl_FragColor.g - 0.04f);
	gl_FragColor.b = max(0.0f, gl_FragColor.b - 0.01f);
}
