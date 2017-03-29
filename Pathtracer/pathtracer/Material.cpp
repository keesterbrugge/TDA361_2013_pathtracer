#include "Material.h"
#include "MCSampling.h"

using namespace chag; 

///////////////////////////////////////////////////////////////////////////////
// Diffuse reflection
///////////////////////////////////////////////////////////////////////////////
float3 DiffuseMaterial::f(const float3 &wi, const float3 &wo, const Intersection &isect) const
{
	return m_reflectance * (1.0f / M_PI); 
}

float3 DiffuseMaterial::sample_f(const float3 &wi, float3 &wo, const Intersection &isect, float &pdf) const
{
	const float3 &n = isect.m_normal; 
	float3 tangent = normalize(perpendicular(n)); 
	float3 bitangent = cross(n, tangent); 
	float3 s = uniformSampleHemisphere(); 
	wo = normalize(s.x * tangent + s.y * bitangent + s.z * n);
	pdf = (1.0f / (2.0f * M_PI));
	return f(wi,wo,isect);  
};

///////////////////////////////////////////////////////////////////////////////
// Perfect specular reflection
///////////////////////////////////////////////////////////////////////////////
float3 SpecularReflectionMaterial::f(const float3 &wi, const float3 &wo, const Intersection &isect) const
{
	return make_vector(0.0f, 0.0f, 0.0f); 
}

float3 SpecularReflectionMaterial::sample_f(const float3 &wi, float3 &wo, const Intersection &isect, float &pdf) const
{
	const float3 &n = isect.m_normal; 
	wo = normalize(2.0f * abs(dot(wi,n)) * n - wi); 
	pdf = sameHemisphere(wi, wo, n) ? abs(dot(wo, n)) : 0.0f;
	return m_reflectance; 
}

///////////////////////////////////////////////////////////////////////////////
// Perfect specular refraction
///////////////////////////////////////////////////////////////////////////////
float3 SpecularRefractionMaterial::f(const float3 &wi, const float3 &wo, const Intersection &isect) const
{
	return make_vector(0.0f, 0.0f, 0.0f); 
}

float3 SpecularRefractionMaterial::sample_f(const float3 &wi, float3 &wo, const Intersection &isect, float &pdf) const
{
	const float3 &n = isect.m_normal; 
	float eta; 
	if(dot(-wi, n) < 0.0f) eta = 1.0f/m_ior; 
	else eta = m_ior; 

	float3 N = dot(-wi, n) < 0.0 ? n : -n; 

	float w = -dot(-wi,N) * eta;
	float k = 1.0f + (w-eta)*(w+eta); 
	if(k < 0.0f) {
		// Total internal reflection
		SpecularReflectionMaterial refMat; 
		refMat.m_reflectance = make_vector(1.0f, 1.0f, 1.0f); 
		Intersection newIntersection = isect; 
		newIntersection.m_normal = N; 
		return refMat.sample_f(wi, wo, newIntersection, pdf); 
	}	
	k = sqrt(k); 
	wo = normalize(-eta*wi + (w-k)*N); 
	pdf = 1.0; 
	return make_vector(1.0f, 1.0f, 1.0f); 
};

///////////////////////////////////////////////////////////////////////////////
// Fresnel Blending
///////////////////////////////////////////////////////////////////////////////
float FresnelBlendMaterial::R(const chag::float3 &wo, const float3 &n) const 
{
	return m_R0 + (1.0f - m_R0) * pow(1.0f - abs(dot(wo,n)), 5.0f); 
}

chag::float3 FresnelBlendMaterial::f(const chag::float3 &wi, const chag::float3 &wo, const Intersection &isect) const 
{
	const float3 &n = isect.m_normal; 
	float _R = R(wi,n); 
	return _R * m_onReflectionMaterial->f(wi, wo, isect) + 
		   (1.0f - _R) * m_onRefractionMaterial->f(wi, wo, isect); 
}

chag::float3 FresnelBlendMaterial::sample_f(const chag::float3 &wi, chag::float3 &wo, const Intersection &isect, float &pdf) const 
{
	const float3 &n = isect.m_normal; 
	if(randf() < R(wi, n)) 
		return m_onReflectionMaterial->sample_f(wi, wo, isect, pdf); 
	else 
		return m_onRefractionMaterial->sample_f(wi, wo, isect, pdf); 
}

///////////////////////////////////////////////////////////////////////////////
// Linear Blending
///////////////////////////////////////////////////////////////////////////////
chag::float3 BlendMaterial::f(const chag::float3 &wi, const chag::float3 &wo, const Intersection &isect) const 
{
	const float3 &n = isect.m_normal; 
	return m_w * m_firstMaterial->f(wi, wo, isect) + (1.0f - m_w) * m_secondMaterial->f(wi, wo, isect); 
}

chag::float3 BlendMaterial::sample_f(const chag::float3 &wi, chag::float3 &wo, const Intersection &isect, float &pdf) const 
{
	const float3 &n = isect.m_normal; 
	if(randf() < m_w) 
		return m_firstMaterial->sample_f(wi, wo, isect, pdf); 
	else 
		return m_secondMaterial->sample_f(wi, wo, isect, pdf); 
}

