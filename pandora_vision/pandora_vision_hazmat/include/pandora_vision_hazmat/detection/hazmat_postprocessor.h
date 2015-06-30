/*********************************************************************
 *
 * Software License Agreement (BSD License)
 *
 *  Copyright (c) 2015, P.A.N.D.O.R.A. Team.
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above
 *     copyright notice, this list of conditions and the following
 *     disclaimer in the documentation and/or other materials provided
 *     with the distribution.
 *   * Neither the name of the P.A.N.D.O.R.A. Team nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 *  FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 *  COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 *  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 *  BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 *  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 *  CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 *  LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 *  ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 *
 * Authors:
 *   Choutas Vassilis
 *   Chatzieleftheriou Eirini <eirini.ch0@gmail.com>
 *********************************************************************/

#ifndef PANDORA_VISION_HAZMAT_DETECTION_HAZMAT_POSTPROCESSOR_H
#define PANDORA_VISION_HAZMAT_DETECTION_HAZMAT_POSTPROCESSOR_H

#include <string>
#include "pandora_common_msgs/GeneralAlertVector.h"
#include "pandora_vision_msgs/HazmatAlert.h"
#include "pandora_vision_msgs/HazmatAlertVector.h"
#include "pandora_vision_common/pandora_vision_interface/vision_postprocessor.h"
#include "pandora_vision_hazmat/detection/hazmat_poi.h"

namespace pandora_vision
{
  class HazmatPostProcessor : public VisionPostProcessor<pandora_vision_msgs::HazmatAlertVector>
  {
    public:
      typedef boost::shared_ptr<pandora_vision_msgs::HazmatAlertVector> HazmatAlertVectorPtr;

      HazmatPostProcessor(const std::string& ns, sensor_processor::Handler* handler);
      virtual ~HazmatPostProcessor();

    virtual bool
      postProcess(const POIsStampedConstPtr& input, const HazmatAlertVectorPtr& output);
  };
}  // namespace pandora_vision

#endif  // PANDORA_VISION_HAZMAT_DETECTION_HAZMAT_POSTPROCESSOR_H
