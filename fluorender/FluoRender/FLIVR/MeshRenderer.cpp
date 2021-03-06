//  
//  For more information, please see: http://software.sci.utah.edu
//  
//  The MIT License
//  
//  Copyright (c) 2004 Scientific Computing and Imaging Institute,
//  University of Utah.
//  
//  
//  Permission is hereby granted, free of charge, to any person obtaining a
//  copy of this software and associated documentation files (the "Software"),
//  to deal in the Software without restriction, including without limitation
//  the rights to use, copy, modify, merge, publish, distribute, sublicense,
//  and/or sell copies of the Software, and to permit persons to whom the
//  Software is furnished to do so, subject to the following conditions:
//  
//  The above copyright notice and this permission notice shall be included
//  in all copies or substantial portions of the Software.
//  
//  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
//  OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
//  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
//  THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
//  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
//  FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
//  DEALINGS IN THE SOFTWARE.
//  

#include "MeshRenderer.h"
#include <iostream>
#include <vector>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_inverse.hpp>

using namespace std;

namespace FLIVR
{
	MshShaderFactory MeshRenderer::msh_shader_factory_;

	MeshRenderer::MeshRenderer(GLMmodel* data)
		: data_(data),
		depth_peel_(0),
		draw_clip_(false),
		limit_(-1),
		light_(true),
		fog_(false),
		alpha_(1.0),
		update_(true)
	{
		glGenBuffers(1, &m_vbo);
		glGenVertexArrays(1, &m_vao);
	}

	MeshRenderer::MeshRenderer(MeshRenderer &copy)
		: data_(copy.data_),
		depth_peel_(copy.depth_peel_),
		draw_clip_(copy.draw_clip_),
		limit_(copy.limit_),
		light_(copy.light_),
		fog_(copy.fog_),
		alpha_(copy.alpha_),
		update_(true)
	{
		glGenBuffers(1, &m_vbo);
		glGenVertexArrays(1, &m_vao);
	}

	MeshRenderer::~MeshRenderer()
	{
		if (glIsBuffer(m_vbo))
			glDeleteBuffers(1, &m_vbo);
		if (glIsVertexArray(m_vao))
			glDeleteVertexArrays(1, &m_vao);
	}

	//clipping planes
	void MeshRenderer::set_planes(vector<Plane*> *p)
	{
		int i;
		if (!planes_.empty())
		{
			for (i=0; i<(int)planes_.size(); i++)
			{
				if (planes_[i])
					delete planes_[i];
			}
			planes_.clear();
		}

		for (i=0; i<(int)p->size(); i++)
		{
			Plane *plane = new Plane(*(*p)[i]);
			planes_.push_back(plane);
		}
	}

	vector<Plane*>* MeshRenderer::get_planes()
	{
		return &planes_;
	}

	void MeshRenderer::update()
	{
		bool bnormal = data_->normals;
		bool btexcoord = data_->texcoords;
		vector<float> verts;

		GLMgroup* group = data_->groups;
		GLMtriangle* triangle = 0;
		while (group)
		{
			for (size_t i=0; i<group->numtriangles; ++i)
			{
				triangle = &(data_->triangles[group->triangles[i]]);
				for (size_t j=0; j<3; ++j)
				{
					verts.push_back(data_->vertices[3*triangle->vindices[j]]);
					verts.push_back(data_->vertices[3*triangle->vindices[j]+1]);
					verts.push_back(data_->vertices[3*triangle->vindices[j]+2]);
					if (bnormal)
					{
						verts.push_back(data_->normals[3*triangle->nindices[j]]);
						verts.push_back(data_->normals[3*triangle->nindices[j]+1]);
						verts.push_back(data_->normals[3*triangle->nindices[j]+2]);
					}
					if (btexcoord)
					{
						verts.push_back(data_->texcoords[2*triangle->tindices[j]]);
						verts.push_back(data_->texcoords[2*triangle->tindices[j]+1]);
					}
				}
			}
			group = group->next;
		}

		glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
		glBufferData(GL_ARRAY_BUFFER, sizeof(float)*verts.size(), &verts[0], GL_STATIC_DRAW);
		glBindVertexArray(m_vao);
		glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
		GLsizei stride = sizeof(float)*(3+(bnormal?3:0)+(btexcoord?2:0));
		glEnableVertexAttribArray(0);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (const GLvoid*)0);
		if (bnormal)
		{
			glEnableVertexAttribArray(1);
			glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, (const GLvoid*)12);
		}
		if (btexcoord)
		{
			if (bnormal)
			{
				glEnableVertexAttribArray(2);
				glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, stride, (const GLvoid*)24);
			}
			else
			{
				glEnableVertexAttribArray(1);
				glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, stride, (const GLvoid*)12);
			}
		}
		glDisableVertexAttribArray(0);
		glBindBuffer(GL_ARRAY_BUFFER, 0);
		glBindVertexArray(0);
	}

	void MeshRenderer::draw()
	{
		if (!data_ || !data_->vertices || !data_->triangles)
			return;

		//set up vertex array object
		if (update_)
		{
			update();
			update_ = false;
		}

		GLint vp[4];
		glGetIntegerv(GL_VIEWPORT, vp);

		ShaderProgram* shader = 0;

		glBindVertexArray(m_vao);
		glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
		glEnableVertexAttribArray(0);

		GLMgroup* group = data_->groups;
		GLint pos = 0;
		bool tex = data_->hastexture==GL_TRUE;
		while (group)
		{
			if (group->numtriangles == 0)
			{
				group = group->next;
				continue;
			}

			//set up shader
			shader = msh_shader_factory_.shader(0,
				depth_peel_, tex, fog_, light_);
			if (shader)
			{
				if (!shader->valid())
					shader->create();
				shader->bind();
			}
			//uniforms
			shader->setLocalParamMatrix(0, glm::value_ptr(m_proj_mat));
			shader->setLocalParamMatrix(1, glm::value_ptr(m_mv_mat));
			if (light_)
			{
				glm::mat4 normal_mat = glm::mat4(glm::inverseTranspose(glm::mat3(m_mv_mat)));
				shader->setLocalParamMatrix(2, glm::value_ptr(normal_mat));
				GLMmaterial* material = &data_->materials[group->material];
				if (material)
				{
					shader->setLocalParam(0, material->ambient[0], material->ambient[1], material->ambient[2], material->ambient[3]);
					shader->setLocalParam(1, material->diffuse[0], material->diffuse[1], material->diffuse[2], material->diffuse[3]);
					shader->setLocalParam(2, material->specular[0], material->specular[1], material->specular[2], material->specular[3]);
					shader->setLocalParam(3, material->shininess, alpha_, 0.0, 0.0);
				}
			}
			else
			{//color
				GLMmaterial* material = &data_->materials[group->material];
				if (material)
					shader->setLocalParam(0, material->diffuse[0], material->diffuse[1], material->diffuse[2], material->diffuse[3]);
				else
					shader->setLocalParam(0, 1.0, 0.0, 0.0, 1.0);//color
				shader->setLocalParam(3, 0.0, alpha_, 0.0, 0.0);//alpha
			}
			if (tex)
			{
				GLMmaterial* material = &data_->materials[group->material];
				if (material)
				{
					glActiveTexture(GL_TEXTURE0);
					glBindTexture(GL_TEXTURE_2D,
						material->textureID);
				}
			}
			if (fog_)
				shader->setLocalParam(8, m_fog_intensity, m_fog_start, m_fog_end, 0.0);

			if (depth_peel_)
				shader->setLocalParam(7, 1.0/double(vp[2]), 1.0/double(vp[3]), 0.0, 0.0);

			//draw
			glDrawArrays(GL_TRIANGLES, pos, (GLsizei)(group->numtriangles*3));
			pos += group->numtriangles*3;
			group = group->next;
		}
		glDisableVertexAttribArray(0);
		glBindBuffer(GL_ARRAY_BUFFER, 0);
		glBindVertexArray(0);

		// Release shader.
		if (shader && shader->valid())
			shader->release();
		//release texture
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, 0);
	}

	void MeshRenderer::draw_wireframe()
	{
		if (!data_ || !data_->vertices || !data_->triangles)
			return;

		//set up vertex array object
		if (update_)
		{
			update();
			update_ = false;
		}

		glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

		ShaderProgram* shader = 0;

		glBindVertexArray(m_vao);
		glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
		glEnableVertexAttribArray(0);

		GLMgroup* group = data_->groups;
		GLint pos = 0;
		int peel = 0;
		bool tex = false;
		bool light = false;
		
		//set up shader
		shader = msh_shader_factory_.shader(0,
			peel, tex, fog_, light);
		if (shader)
		{
			if (!shader->valid())
				shader->create();
			shader->bind();
		}
		//uniforms
		shader->setLocalParamMatrix(0, glm::value_ptr(m_proj_mat));
		shader->setLocalParamMatrix(1, glm::value_ptr(m_mv_mat));
		GLMmaterial* material = &data_->materials[0];
		if (material)
			shader->setLocalParam(0, material->diffuse[0], material->diffuse[1], material->diffuse[2], material->diffuse[3]);
		else
			shader->setLocalParam(0, 1.0, 0.0, 0.0, 1.0);
		shader->setLocalParam(3, 0.0, 1.0, 0.0, 0.0);//alpha
		if (fog_)
			shader->setLocalParam(8, m_fog_intensity, m_fog_start, m_fog_end, 0.0);


		while (group)
		{
			if (group->numtriangles == 0)
			{
				group = group->next;
				continue;
			}

			//draw
			glDrawArrays(GL_TRIANGLES, pos, (GLsizei)(group->numtriangles*3));
			pos += group->numtriangles*3;
			group = group->next;
		}
		glDisableVertexAttribArray(0);
		glBindBuffer(GL_ARRAY_BUFFER, 0);
		glBindVertexArray(0);

		// Release shader.
		if (shader && shader->valid())
			shader->release();
		glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	}

	void MeshRenderer::draw_integer(unsigned int name)
	{
		if (!data_ || !data_->vertices || !data_->triangles)
			return;

		//set up vertex array object
		if (update_)
		{
			update();
			update_ = false;
		}

		ShaderProgram* shader = 0;

		glBindVertexArray(m_vao);
		glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
		glEnableVertexAttribArray(0);

		GLMgroup* group = data_->groups;
		GLint pos = 0;

		//set up shader
		shader = msh_shader_factory_.shader(1,
			0, false, false, false);
		if (shader)
		{
			if (!shader->valid())
				shader->create();
			shader->bind();
		}
		//uniforms
		shader->setLocalParamMatrix(0, glm::value_ptr(m_proj_mat));
		shader->setLocalParamMatrix(1, glm::value_ptr(m_mv_mat));
		shader->setLocalParamUInt(0, name);

		while (group)
		{
			if (group->numtriangles == 0)
			{
				group = group->next;
				continue;
			}

			//draw
			glDrawArrays(GL_TRIANGLES, pos, (GLsizei)(group->numtriangles*3));
			pos += group->numtriangles*3;
			group = group->next;
		}
		glDisableVertexAttribArray(0);
		glBindBuffer(GL_ARRAY_BUFFER, 0);
		glBindVertexArray(0);

		// Release shader.
		if (shader && shader->valid())
			shader->release();
	}

} // namespace FLIVR