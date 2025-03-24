#version 330 core
uniform sampler2D Texture0;

in vec2 vTexCoord;

in vec3 fragNor;
in vec3 lightDir;

out vec4 Outcolor;

void main() {
	vec4 texColor0 = texture(Texture0, vTexCoord);

	vec3 normal = normalize(fragNor);
	vec3 light = normalize(lightDir);

	// Diffuse lighting calculation
    float diff = max(dot(normal, light), 0.0);
    vec3 diffuse = diff * texColor0.rgb;
	

	// discard pixels
	//vec4 curColor = vec4(diffuse, 1.0);
	//vec3 discardColor = vec3(0.0, 1.0, 0.0);
	//float discardThresh = 0.8;

	//if (distance(curColor.rgb, discardColor) < discardThresh) {
    //     discard; // Discard the fragment
    //}
	
	Outcolor = vec4(diffuse, 1.0);


	//Outcolor = vec4(vTexCoord.s, vTexCoord.t, 0, 1);
}