/* Copyright (c) 2018 Anakin Authors, Inc. All Rights Reserved.

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0
   
   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License. 
*/

#ifndef ANAKIN_SABER_FUNCS_IMPL_X86_SABER_UNPOOL_H
#define ANAKIN_SABER_FUNCS_IMPL_X86_SABER_UNPOOL_H

#include "saber/funcs/impl/impl_unpool.h"

namespace anakin{

namespace saber{

template <DataType OpDtype>
class SaberUnpool<X86, OpDtype>:
    public ImplBase<X86, OpDtype, PoolingParam<X86>> {

public:
    typedef Tensor<X86> DataTensor_in;
    typedef Tensor<X86> DataTensor_out;
    typedef Tensor<X86> OpTensor;

    typedef typename DataTrait<X86, OpDtype>::Dtype InDataType;
    typedef typename DataTrait<X86, OpDtype>::Dtype OutDataType;
    typedef typename DataTrait<X86, OpDtype>::Dtype OpDataType;

    SaberUnpool()
    {}

    ~SaberUnpool() {

    }

    virtual SaberStatus init(const std::vector<DataTensor_in*>& inputs,
                             std::vector<DataTensor_out*>& outputs,
                             PoolingParam<X86> &param,
                             Context<X86> &ctx)  {
        this->_ctx = &ctx;
        return create(inputs, outputs, param, ctx);
    }

    virtual SaberStatus create(const std::vector<DataTensor_in*>& inputs,
                               std::vector<DataTensor_out*>& outputs,
                               PoolingParam<X86> &param,
                               Context<X86> &ctx)  {
        Shape out_stride = outputs[0]->get_stride();
        Shape in_stride = inputs[0]->get_stride();
        int in_n_index = inputs[0]->num_index();
        int in_c_index = inputs[0]->channel_index();
        int out_n_index = outputs[0]->num_index();
        int out_c_index = outputs[0]->channel_index();
        _in_n_stride = in_stride[in_n_index];
        _in_c_stride = in_stride[in_c_index];
        _out_n_stride = out_stride[out_n_index];
        _out_c_stride = out_stride[out_c_index];
        return SaberSuccess;
    }

    virtual SaberStatus dispatch(const std::vector<DataTensor_in*>& inputs,
                                 std::vector<DataTensor_out*>& outputs,
                                 PoolingParam<X86> &param);

private:
    int _in_n_stride;
    int _in_c_stride;
    int _out_n_stride;
    int _out_c_stride;
};
template class SaberUnpool<X86, AK_FLOAT>;
}

}

#endif //ANAKIN_SABER_FUNCS_IMPL_X86_SABER_UNPOOL_H
