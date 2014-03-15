////////////////////////////////////////////////////////////////////////////////
// Filename: graphicsclass.cpp
////////////////////////////////////////////////////////////////////////////////
#include "Particleclass.h"


// Spawn new particles
void Particleclass::SpawnNewParticles(float elapsedSeconds)
{
	// Check if the interval was accomplished
	newParticlesTimer += elapsedSeconds;
	if (!newParticlesCreated && emissionRate > 0 && newParticlesTimer >= spawnParticlesInterval)
	{
		// Initialize variables
		D3DXVECTOR3 Random;
		newParticlesArr = new PARTICLE_VERTEX[emissionRate];
		// Initialize vertex array to zeros at first.
		memset(newParticlesArr, 0, (sizeof(PARTICLE_VERTEX)* emissionRate));
		// Loop the particle creation
		for (int i = 0; i < emissionRate; i++)
		{
			// Generate new random value
			Random = D3DXVECTOR3((float)((rand() % 2000 - 1000) / 1000.0f), 
								 (float)((rand() % 2000 - 1000) / 1000.0f),
								 (float)((rand() % 2000 - 1000) / 1000.0f));
			// create particle struct
			PARTICLE_VERTEX v;
			v.pos = position;
			v.vel =  D3DXVECTOR3(velocity.x + velocity.x * (Random.x * 4),
								 velocity.y + velocity.y * (Random.y * 4),
								 velocity.z + velocity.z * (Random.z * 4));
			v.Timer = lifeTime;
			v.Type = 1;
			// Temp store the particle
			newParticlesArr[i] = v;
		}
		// Reset timer
		newParticlesTimer = 0; 
		// Set creation flag
		newParticlesCreated = true;
	}
}

// Render the particle system
void Particleclass::Render(float elapsedMiliSeconds)
{
	// Common variables
	D3DXMATRIX mView;
	D3DXMATRIX mProj;
	D3DXMATRIX mWorldView;
	D3DXMATRIX mWorldViewProj;

	// Get the projection & view matrix from the camera class
	m_D3D->GetProjectionMatrix(mProj);
	mView = m_camera->GetViewMatrix();

	// Set IA parameters
	ID3D11Buffer *pBuffers[1] = { g_pParticleDrawFrom };
	UINT stride[1] = { sizeof(PARTICLE_VERTEX) };
	UINT offset[1] = { 0 };
	m_D3D->GetDeviceContext()->IASetVertexBuffers(0, 1, pBuffers, stride, offset);
	m_D3D->GetDeviceContext()->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_POINTLIST);

	// Set Effects Parameters
	D3DXMatrixMultiply(&mWorldViewProj, &mView, &mProj);
	g_pmWorldViewProj->SetMatrix((float*)mWorldViewProj);
	g_pDiffuseTex->SetResource(g_pParticleDefaultTexRV);
	D3DXMATRIX mInvView;
	D3DXMatrixInverse(&mInvView, NULL, &mView);
	g_pmInvView->SetMatrix((float*)&mInvView);
	// Set particle color
	g_Color->SetFloatVector((float*)&color);
	
	// Draw
	D3DX11_TECHNIQUE_DESC techDesc;
	g_pRenderParticles->GetDesc(&techDesc);

	for (UINT p = 0; p < techDesc.Passes; ++p)
	{
		g_pRenderParticles->GetPassByIndex(p)->Apply(0, m_D3D->GetDeviceContext());
		m_D3D->GetDeviceContext()->DrawAuto();
	}

}

// Update the particle system (advance the particle system)
void Particleclass::Update(float elapsedSeconds)
{
	// Update total elapsed time
	m_TotalTimeElapsed += elapsedSeconds*1000; // Miliseconds, so the RNG functions inside the GPU get diverse values

	// CPU PARTICLE CREATION
	// Spawn new particles if needed
	SpawnNewParticles(elapsedSeconds);
	
	// Set Effects Parameters
	D3DXVECTOR4 vGravity(gravity.x, gravity.y, gravity.z, 0);
	g_pfGlobalTime->SetFloat(m_TotalTimeElapsed);
	g_pfElapsedTime->SetFloat(elapsedSeconds);
	vGravity *= elapsedSeconds;
	// Gravity
	g_pvFrameGravity->SetFloatVector((float*)&vGravity);
	// Wind
	if (windEnabled) g_pWind->SetFloatVector((float*)&wind);
	else  g_pWind->SetFloatVector((float*)&D3DXVECTOR3(0,0,0));
	g_pRandomTex->SetResource(g_pRandomTexRV);
	g_pSecondsPerParticle->SetFloat(spawnParticlesInterval);
	g_pRate->SetInt(emissionRate);
	g_pNumEmber1s->SetInt(100);
	g_pMaxEmber2s->SetFloat(15.0f);
	// Set emitter parameters
	g_pEmitterPos->SetFloatVector((float*)&position);
	g_pEmitterVel->SetFloatVector((float*)&velocity);
	g_pLifeTime->SetFloat(lifeTime);

	// Set IA parameters
	ID3D11Buffer *pBuffers[1];
	if (firstFrame)
		pBuffers[0] = g_pParticleStart;
	else
		pBuffers[0] = g_pParticleDrawFrom;
	UINT stride[1] = { sizeof(PARTICLE_VERTEX) };
	UINT offset[1] = { 0 };
	m_D3D->GetDeviceContext()->IASetVertexBuffers(0, 1, pBuffers, stride, offset);
	m_D3D->GetDeviceContext()->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_POINTLIST);

	// Point to the correct output buffer
	pBuffers[0] = g_pParticleStreamTo;
	m_D3D->GetDeviceContext()->SOSetTargets(1, pBuffers, offset);

	// Command the "render" operation. This will translate in the actual particles update
	D3DX11_TECHNIQUE_DESC techDesc;
	g_pAdvanceParticles->GetDesc(&techDesc);
	for (UINT p = 0; p < techDesc.Passes; ++p)
	{
		g_pAdvanceParticles->GetPassByIndex(p)->Apply(0, m_D3D->GetDeviceContext());
		if (firstFrame)
			m_D3D->GetDeviceContext()->Draw(1, 0);
		else
			m_D3D->GetDeviceContext()->DrawAuto();
	}


	// Get back to normal
	pBuffers[0] = NULL;
	m_D3D->GetDeviceContext()->SOSetTargets(1, pBuffers, offset);

	// CPU PARTICLE CREATION
	// Add new particles if they were created
	if (!newParticlesCreated)
	{
		// Map to the buffer and render the new particles to add them to the pipeline
		D3D11_MAPPED_SUBRESOURCE mappedResource;
		PARTICLE_VERTEX* dataPtr;
		
		m_D3D->GetDeviceContext()->Map(g_pNewParticles, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
		dataPtr = (PARTICLE_VERTEX*)mappedResource.pData;

		// Copy the data into the vertex buffer.
		memcpy(dataPtr, (void*)newParticlesArr, (sizeof(PARTICLE_VERTEX)* emissionRate));

		// Unmap the buffer
		m_D3D->GetDeviceContext()->Unmap(g_pNewParticles, 0);
	
		// Set the buffer as the input
		pBuffers[0] = g_pNewParticles;
		UINT stride[1] = { sizeof(PARTICLE_VERTEX) };
		UINT offset[1] = { 0 };
		m_D3D->GetDeviceContext()->IASetVertexBuffers(0, 1, pBuffers, stride, offset);
		m_D3D->GetDeviceContext()->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_POINTLIST);

		// Point to the correct output buffer
		pBuffers[0] = g_pParticleStreamTo;
		m_D3D->GetDeviceContext()->SOSetTargets(1, pBuffers, offset);

		// Draw the buffers
		D3DX11_TECHNIQUE_DESC techDesc;
		g_pAdvanceParticles->GetDesc(&techDesc);
		for (UINT p = 0; p < techDesc.Passes; ++p)
		{
			g_pAdvanceParticles->GetPassByIndex(p)->Apply(0, m_D3D->GetDeviceContext());
			m_D3D->GetDeviceContext()->DrawAuto();
		}

		// Get back to normal
		pBuffers[0] = NULL;
		m_D3D->GetDeviceContext()->SOSetTargets(1, pBuffers, offset);

		// Free the particle array memory
		delete (newParticlesArr);

		// Reset variables
		newParticlesCreated = false;
	}

	// Swap particle buffers
	ID3D11Buffer* pTemp = g_pParticleDrawFrom;
	g_pParticleDrawFrom = g_pParticleStreamTo;
	g_pParticleStreamTo = pTemp;

	// Remove the first frame flag
	if (firstFrame)
		firstFrame = false;
}

// Command the particle system calculations (update and render)
void Particleclass::Draw(float elapsedMiliSeconds,D3DXMATRIX worldMatrix, D3DXMATRIX viewMatrix,D3DXMATRIX projectionMatrix)
{
	// Timescale
	float elapsedSeconds = elapsedMiliSeconds / 1000.0f;

	// Set Vertex Input Layout
	m_D3D->GetDeviceContext()->IASetInputLayout(g_pParticleVertexLayout);

	// Simulation
	if (State == ParticleSystemState::PLAYING)
		Update(elapsedSeconds);

	// Render
	Render(elapsedSeconds);
}

//--------------------------------------------------------------------------------------
// This helper function creates 3 vertex buffers.  The first is used to seed the
// particle system.  The second two are used as output and intput buffers alternatively
// for the GS particle system.  Since a buffer cannot be both an input to the GS and an
// output of the GS, we must ping-pong between the two.
// NOTE: the current Debug version adds a new buffer, this is the one used to create new particles
// inside the CPU, and pass them to the GPU
//--------------------------------------------------------------------------------------
bool Particleclass::CreateParticleBuffer(ID3D11Device* pd3dDevice)
{
	HRESULT hr = S_OK;
	D3D11_BUFFER_DESC vbdesc =
	{
		1 * sizeof(PARTICLE_VERTEX),
		D3D11_USAGE_DEFAULT,
		D3D11_BIND_VERTEX_BUFFER,
		0,
		0
	};
	// Set the initializer buffer data
	D3D11_SUBRESOURCE_DATA vbInitData;
	ZeroMemory(&vbInitData, sizeof(D3D10_SUBRESOURCE_DATA));

	PARTICLE_VERTEX vertStart =
	{
		D3DXVECTOR3(0,-999999, 0),	// The emitter particle must be outside the field of vision
		D3DXVECTOR3(0, 0, 0),
		float(0),
		UINT(0),
	};

	vbInitData.pSysMem = &vertStart;
	vbInitData.SysMemPitch = sizeof(PARTICLE_VERTEX);

	// Create the initializer buffer
	hr = pd3dDevice->CreateBuffer(&vbdesc, &vbInitData, &g_pParticleStart);
	if (FAILED(hr))return false;

	// CPU PARTICLE CREATION
	// Create the new particles buffer
	D3D11_BUFFER_DESC newPartDesc;
	newPartDesc.Usage = D3D11_USAGE_DYNAMIC;
	newPartDesc.ByteWidth = MAX_PARTICLES_PER_FRAME * sizeof(PARTICLE_VERTEX);
	newPartDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	newPartDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	newPartDesc.MiscFlags = 0;
	newPartDesc.StructureByteStride = 0;
	hr = pd3dDevice->CreateBuffer(&newPartDesc, NULL, &g_pNewParticles);
	if (FAILED(hr))return false;

	// Create the interchangeables buffers
	vbdesc.ByteWidth = maxParticles*sizeof(PARTICLE_VERTEX);
	vbdesc.BindFlags |= D3D11_BIND_STREAM_OUTPUT;

	hr = pd3dDevice->CreateBuffer(&vbdesc, NULL, &g_pParticleDrawFrom);
	if (FAILED(hr))return false;
	hr = pd3dDevice->CreateBuffer(&vbdesc, NULL, &g_pParticleStreamTo);
	if (FAILED(hr))return false;

	return true;
}

// Initialize the particle system shader (ie, the meat of the particle system)
bool Particleclass::InitializeShader(ID3D11Device* device, HWND hwnd,LPCSTR fxFilename)
{
	// Initial variables declaration
	HRESULT result;
	ID3D10Blob * errorMessage;
	ID3D10Blob* effectShaderBuffer;
	// Compile the effect shader code.
	result = D3DX11CompileFromFile(fxFilename, NULL, NULL, NULL, "fx_5_0", D3DCOMPILE_ENABLE_STRICTNESS, 0, NULL,
		&effectShaderBuffer, &errorMessage, NULL);
	// Compiling error check
	if (FAILED(result))
	{
		// If the shader failed to compile it should have writen something to the error message.
		if (errorMessage)
			OutputShaderErrorMessage(errorMessage, hwnd, fxFilename);
		// If there was nothing in the error message then it simply could not find the shader file itself.
		else
			MessageBox(hwnd, fxFilename, "Missing Shader File", MB_OK);
		return false;
	}
	// Create the effect buffer
	result = D3DX11CreateEffectFromMemory(effectShaderBuffer->GetBufferPointer(), effectShaderBuffer->GetBufferSize(), NULL, device, &pEffect);
	// Creation error check
	if (FAILED(result) || pEffect == nullptr)
	{
		std::stringstream stream;stream << "Error Code::" << HRESULT_CODE(result);
		MessageBox(NULL, stream.str().c_str(), "Effect Creation Error", MB_OK);
		return false;
	}

	// Obtain the technique handles
	g_pRenderParticles = pEffect->GetTechniqueByName("RenderParticles");
	if (!g_pRenderParticles->IsValid())return false;
	g_pAdvanceParticles = pEffect->GetTechniqueByName("AdvanceParticles");
	if (!g_pAdvanceParticles->IsValid())return false;

	// Obtain the parameter handles
	g_pEmitterPos = pEffect->GetVariableByName("g_emitterPosition")->AsVector();
	g_pEmitterVel = pEffect->GetVariableByName("g_emitterVelocity")->AsVector();
	g_pLifeTime = pEffect->GetVariableByName("g_lifeTime")->AsScalar();
	g_pRate = pEffect->GetVariableByName("g_emitterRate")->AsScalar();
	g_pWind = pEffect->GetVariableByName("g_wind")->AsVector();
	g_Color = pEffect->GetVariableByName("g_color")->AsVector();
	g_pmWorldViewProj = pEffect->GetVariableByName("g_mWorldViewProj")->AsMatrix();
	g_pmInvView = pEffect->GetVariableByName("g_mInvView")->AsMatrix();
	g_pfGlobalTime = pEffect->GetVariableByName("g_fGlobalTime")->AsScalar();
	g_pfElapsedTime = pEffect->GetVariableByName("g_fElapsedTime")->AsScalar();
	g_pDiffuseTex = pEffect->GetVariableByName("g_txDiffuse")->AsShaderResource();
	g_pRandomTex = pEffect->GetVariableByName("g_txRandom")->AsShaderResource();
	g_pSecondsPerParticle = pEffect->GetVariableByName("g_fSecondsPerParticle")->AsScalar();
	g_pNumEmber1s = pEffect->GetVariableByName("g_iNumEmber1s")->AsScalar();
	g_pMaxEmber2s = pEffect->GetVariableByName("g_fMaxEmber2s")->AsScalar();
	g_pvFrameGravity = pEffect->GetVariableByName("g_vFrameGravity")->AsVector();

	// Create our vertex input layout
	const D3D11_INPUT_ELEMENT_DESC layout[] =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "NORMAL"  , 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "TIMER"   , 0, DXGI_FORMAT_R32_FLOAT      , 0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "TYPE"    , 0, DXGI_FORMAT_R32_UINT       , 0, 28, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	};
	D3DX11_PASS_DESC PassDesc;
	g_pAdvanceParticles->GetPassByIndex(0)->GetDesc(&PassDesc);
	result = device->CreateInputLayout(layout, sizeof(layout) / sizeof(layout[0]), PassDesc.pIAInputSignature, PassDesc.IAInputSignatureSize, &g_pParticleVertexLayout);
	// Error check
	if (FAILED(result) || g_pParticleVertexLayout == nullptr)
		return false;

	// Set effect variables as needed
	D3DXCOLOR colorMtrlDiffuse(1.0f, 1.0f, 1.0f, 1.0f);
	D3DXCOLOR colorMtrlAmbient(0.35f, 0.35f, 0.35f, 0);

	// Create the emmiter particle buffer, and the interchangeable buffers used during rendering
	if (!CreateParticleBuffer(device))
		return false;

	// Load the default Particle Texture
	result = D3DX11CreateShaderResourceViewFromFile(device, "Dot.png", NULL, NULL, &g_pParticleDefaultTexRV, NULL);
	if (FAILED(result) || g_pParticleDefaultTexRV == nullptr)
		return false;

	// Create the random texture that fuels our random vector generator in the effect
	if (!CreateRandomTexture(device))
		return false;

	// Return correct initialization
	return true;
}

// Initialize and start the particle system
bool Particleclass::Initialize(HWND hwnd)
{
	// Initialize the shader
	if (!InitializeShader(m_D3D->GetDevice(), hwnd, "ParticleEffect.fx"))
		return false;

	// Mark as first frame
	firstFrame = true;
	m_TotalTimeElapsed = 0;
	return true;
}

// Initialize all variables to null due safety reasons
Particleclass::Particleclass(CameraClass *refCamera,D3DClass *refGraphics,HWND owner)
{
	// Set common references
	m_camera = refCamera;
	m_D3D = refGraphics;

	// Initialize system variables
	maxParticles = 10000000;

	color = D3DXCOLOR(1,1,1,1);

	position.x=0;
	position.y=5;
	position.z=0;

	velocity.x=0;
	velocity.y=40;
	velocity.z=0;

	wind.x = 10;
	wind.y = 0;
	wind.z = 0;
	windEnabled = false;


	acceleration = 1.0f;

	gravity.x = 0;
	gravity.y = -9.8f;
	gravity.z = 0;

	emissionRate = 5;
	spawnParticlesInterval = 0.0f;

	lifeTime = 2.0f;

	startSize = 0.02f;

	endSize = 0.07f;

	State = ParticleSystemState::UNSTARTED;

	newParticlesCreated = false;

	// Initialize buffers to null
	g_pParticleVertexLayout = NULL;
	pEffect = NULL;
    g_pParticleStart = NULL;
	g_pNewParticles = NULL;
    g_pParticleStreamTo = NULL;
    g_pParticleDrawFrom = NULL;

	// Initialize variables
	newParticles = 0;
	m_TotalTimeElapsed = 0;
	newParticlesTimer = 0;

	// Initialize the particle system
	if(!Initialize(owner))
		MessageBox(NULL, "Could not initialize the particle system", "Error", MB_OK);
}

Particleclass::~Particleclass()
{
	SAFE_RELEASE(g_pParticleVertexLayout);
    SAFE_RELEASE( g_pParticleStart );
    SAFE_RELEASE( g_pParticleStreamTo );
    SAFE_RELEASE( g_pParticleDrawFrom );
	SAFE_RELEASE(g_pParticleVertexLayout);
	SAFE_RELEASE(g_pParticleStart);
	SAFE_RELEASE(g_pNewParticles);
	SAFE_RELEASE(g_pParticleStreamTo);
	SAFE_RELEASE(g_pParticleDrawFrom);
	SAFE_RELEASE(g_pParticleDefaultTexRV);
	SAFE_RELEASE(g_pRandomTexture);
	SAFE_RELEASE(g_pRandomTexRV);
}

// Errror check
void Particleclass::OutputShaderErrorMessage(ID3D10Blob* errorMessage, HWND hwnd, LPCSTR shaderFilename)
{
	char* compileErrors;
	unsigned long bufferSize, i;
	ofstream fout;


	// Get a pointer to the error message text buffer.
	compileErrors = (char*)(errorMessage->GetBufferPointer());

	// Get the length of the message.
	bufferSize = errorMessage->GetBufferSize();

	// Open a file to write the error message to.
	fout.open("shader-error.txt");

	// Write out the error message.
	for(i=0; i<bufferSize; i++)
	{
		fout << compileErrors[i];
	}

	// Close the file.
	fout.close();

	// Release the error message.
	errorMessage->Release();
	errorMessage = 0;

	// Pop a message up on the screen to notify the user to check the text file for compile errors.
	MessageBox(hwnd, "Error compiling shader.  Check shader-error.txt for message.", shaderFilename, MB_OK);

	return;
}


//--------------------------------------------------------------------------------------
// This helper function creates a 1D texture full of random vectors.  The shader uses
// the current time value to index into this texture to get a random vector.
//--------------------------------------------------------------------------------------
bool Particleclass::CreateRandomTexture(ID3D11Device* pd3dDevice)
{
	HRESULT hr = S_OK;

	int iNumRandValues = 1024;
	srand(timeGetTime());
	// Create the data
	D3D11_SUBRESOURCE_DATA InitData;
	InitData.pSysMem = new float[iNumRandValues * 4];
	// Out of memory check
	if (!InitData.pSysMem)return false;
	// Get the memory space
	InitData.SysMemPitch = iNumRandValues * 4 * sizeof(float);
	InitData.SysMemSlicePitch = iNumRandValues * 4 * sizeof(float);
	// Start the random vector creation
	for (int i = 0; i<iNumRandValues * 4; i++)
	{
		((float*)InitData.pSysMem)[i] = float((rand() % 10000) - 5000);
	}

	// Create the texture
	D3D11_TEXTURE1D_DESC dstex;
	dstex.Width = iNumRandValues;
	dstex.MipLevels = 1;
	dstex.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
	dstex.Usage = D3D11_USAGE_DEFAULT;
	dstex.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	dstex.CPUAccessFlags = 0;
	dstex.MiscFlags = 0;
	dstex.ArraySize = 1;
	hr = pd3dDevice->CreateTexture1D(&dstex, &InitData, &g_pRandomTexture);
	
	// Error check
	if (FAILED(hr))
		return false;
	
	// Clear the temporal memory
	SAFE_DELETE_ARRAY(InitData.pSysMem);

	// Create the resource view
	D3D11_SHADER_RESOURCE_VIEW_DESC SRVDesc;
	ZeroMemory(&SRVDesc, sizeof(SRVDesc));
	SRVDesc.Format = dstex.Format;
	SRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE1D;
	SRVDesc.Texture2D.MipLevels = dstex.MipLevels;
	hr = pd3dDevice->CreateShaderResourceView(g_pRandomTexture, &SRVDesc, &g_pRandomTexRV);
	// Error check
	if (FAILED(hr))
		return false;

	return true;
}


// Start the particle system run
void Particleclass::Start()
{
	// Check if the particle system was initialized properly, and if it wasn't already playing 
	if (m_D3D == nullptr || State == ParticleSystemState::PLAYING) return;

	// Set the play state to enable the system play
	State = ParticleSystemState::PLAYING;
}
// Pause the particle system
void Particleclass::Pause()
{
	if (State != ParticleSystemState::PLAYING) return;
	State = ParticleSystemState::PAUSED;
}

// Reset the particle system
void Particleclass::Reset()
{
	if (State == ParticleSystemState::UNSTARTED) return;
	State = ParticleSystemState::UNSTARTED;
}



Particleclass::Particleclass(const Particleclass& other)
{

}
