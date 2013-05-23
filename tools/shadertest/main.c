/*
 * main.c
 * 
 * Copyright 2013 Klaus Gusenleitner <klaus.gusenleitner@gmail.com>
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301, USA.
 * 
 * 
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <time.h>
#include <unistd.h>
#include "digout.h"
#include "shader.h"


uint16_t gTimeMs16 = 0;

int main(void) {

   uint32_t timeMs32 = 0;

   DigOutInit();
   ShaderInit();
   
   ShaderSetConfig(eShader0, eDigOut0, eDigOut1, 30000, 25000);

   while (1) {
      timeMs32++;
	  gTimeMs16++;
      ShaderCheck();
      if (timeMs32 == 100) {
    	  ShaderSetPosition(eShader0, 50);
      }
      if (timeMs32 == 15140) {
    	  // shaderStop while Shader is in State eStateExit
    	  ShaderSetAction(eShader0, eShaderStop);
      }
      if (timeMs32 == 16000) {
    	  ShaderSetAction(eShader0, eShaderClose);
      }
      if (timeMs32 == 50000) {
    	  ShaderSetPosition(eShader0, 99);
      }
      if (timeMs32 == 85000) {
    	  ShaderSetAction(eShader0, eShaderOpen);
      }
      if (timeMs32 == 120000) {
    	  ShaderSetPosition(eShader0, 90);
      }
      if (timeMs32 == 121000) {
    	  // change setPostion, no direction change
    	  ShaderSetPosition(eShader0, 80);
      }
      if (timeMs32 == 130000) {
    	  ShaderSetPosition(eShader0, 70);
      }
      if (timeMs32 == 132000) {
    	  // change setPostion, direction change while closing
    	  ShaderSetPosition(eShader0, 80);
      }
      if (timeMs32 == 140000) {
    	  ShaderSetPosition(eShader0, 90);
      }
      if (timeMs32 == 142000) {
    	  // change setPostion, direction change while opening
    	  ShaderSetPosition(eShader0, 80);
      }

   }
   
   return 0;
}
