#version 330 core 

out vec4 color;

uniform vec3 MatAmb;
uniform vec3 MatDif;
uniform vec3 MatSpec;
uniform float MatShine;

//interpolated normal and light vector in camera space
in vec3 fragNor;
in vec3 lightDir;

//position of the vertex in camera space
in vec3 EPos;

void main()
{
	//you will need to work with these for lighting
	vec3 normal = normalize(fragNor);
	vec3 light = normalize(lightDir);

	// View direction
    vec3 viewDir = normalize(-EPos);
    
    // Ambient lighting calculation
    vec3 ambient = MatAmb;

    // Diffuse lighting calculation
    float diff = max(dot(normal, light), 0.0);
    vec3 diffuse = diff * MatDif;

    // Blinn-Phong specular lighting calculation
    vec3 halfVec = normalize(light + viewDir);
    float spec = pow(max(dot(normal, halfVec), 0.0), MatShine);
    vec3 specular = spec * MatSpec;

    // Combine all lighting components
    vec3 finalColor = ambient + diffuse + specular;
    // should actually multiply the diffuse by the light color

	color = vec4(finalColor, 1.0);
}
