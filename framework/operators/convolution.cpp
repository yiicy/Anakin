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
#include "framework/operators/convolution.h"

namespace anakin {

namespace ops {

#define INSTANCE_CONVOLUTION(Ttype, Ptype) \
template<> \
void Convolution<Ttype, Ptype>::operator()(OpContext<Ttype>& ctx, \
    const std::vector<Tensor4dPtr<Ttype> >& ins, \
    std::vector<Tensor4dPtr<Ttype> >& outs) { \
    auto* impl = static_cast<ConvolutionHelper<Ttype, Ptype>*>(this->_helper); \
    auto& param = static_cast<ConvolutionHelper<Ttype, Ptype>*> \
                  (this->_helper)->_param_conv; \
    SABER_CHECK(impl->_funcs_conv(ins, outs, param, ctx));\
}

template<typename Ttype, Precision Ptype>
Status ConvolutionHelper<Ttype, Ptype>::InitParam() {
    DLOG(WARNING) << "Parsing Convolution op parameter.";
    auto group = GET_PARAMETER(int, group);
    auto bias_term = GET_PARAMETER(bool, bias_term);
    auto padding = GET_PARAMETER(PTuple<int>, padding);
    auto strides = GET_PARAMETER(PTuple<int>, strides);
    auto dilation_rate = GET_PARAMETER(PTuple<int>, dilation_rate);
    auto filter_num = GET_PARAMETER(int, filter_num);
    auto kernel_size = GET_PARAMETER(PTuple<int>, kernel_size);
    auto axis = GET_PARAMETER(int, axis);

	using pblock_type = PBlock<Ttype>;
    auto weights = GET_PARAMETER(pblock_type, weight_1);
    // resize weights scale
    auto& w = weights.h_tensor();
    if (w.get_scale().size() == 1){
        float scale_tmp = w.get_scale()[0];
        std::vector<float> w_scale(filter_num, scale_tmp);
        w.set_scale(w_scale);
    }
    if (bias_term) {
        auto bias = GET_PARAMETER(pblock_type, weight_2);
        saber::ConvParam<Ttype> conv_param(group, padding[0], padding[1],
                strides[0], strides[1], dilation_rate[0], dilation_rate[1],
                &(weights.d_tensor()), &(bias.d_tensor()));
        _param_conv = conv_param;
    } else {
        Tensor4d<Ttype>* bias = new Tensor4d<Ttype>();;
        saber::ConvParam<Ttype> conv_param(group, padding[0], padding[1],
                strides[0], strides[1], dilation_rate[0], dilation_rate[1],
                &(weights.d_tensor()), bias);
        _param_conv = conv_param;
    }

    return Status::OK();
}

template<typename Ttype, Precision Ptype>
Status ConvolutionHelper<Ttype, Ptype>::Init(OpContext<Ttype>& ctx,
        const std::vector<Tensor4dPtr<Ttype> >& ins,
        std::vector<Tensor4dPtr<Ttype> >& outs) {
    auto group = GET_PARAMETER(int, group);
    auto strides = GET_PARAMETER(PTuple<int>, strides);
    auto weights = GET_PARAMETER(PBlock<Ttype>, weight_1);
    auto bias_term = GET_PARAMETER(bool, bias_term);

    //different device pleace change here..
#ifdef AMD_GPU
    saber::ImplEnum impl_e = SABER_IMPL;
#else
    saber::ImplEnum impl_e = VENDER_IMPL;
    if (std::is_same<Ttype, X86>::value || std::is_same<Ttype, ARM>::value) {
        impl_e = SABER_IMPL;
    }
    if (std::is_same<Ttype, NV>::value && Ptype == Precision::INT8) {
        impl_e = SABER_IMPL;
    }
    if (std::is_same<Ttype, MLU>::value) {
        impl_e = SABER_IMPL;
    }
    bool use_k1s1p0 = (Ptype == Precision::FP32);
    use_k1s1p0 = use_k1s1p0 && (_param_conv.weight()->height() == 1);
    use_k1s1p0 = use_k1s1p0 && (_param_conv.weight()->width() == 1);
    use_k1s1p0 = use_k1s1p0 && (_param_conv.pad_h == 0);
    use_k1s1p0 = use_k1s1p0 && (_param_conv.pad_w == 0);
    use_k1s1p0 = use_k1s1p0 && (_param_conv.stride_h == 1);
    use_k1s1p0 = use_k1s1p0 && (_param_conv.stride_w == 1);
    use_k1s1p0 = use_k1s1p0 && (_param_conv.dilation_h == 1);
    use_k1s1p0 = use_k1s1p0 && (_param_conv.dilation_w == 1);
    use_k1s1p0 = use_k1s1p0 && (_param_conv.group == 1);
    use_k1s1p0 = use_k1s1p0 && (_param_conv.bias()->valid_size() > 0);
    bool use_k3s1d1 = (Ptype == Precision::FP32);
    use_k3s1d1 = use_k3s1d1 && (_param_conv.weight()->height() == 3);
    use_k3s1d1 = use_k3s1d1 && (_param_conv.weight()->width() == 3);
    use_k3s1d1 = use_k3s1d1 && (_param_conv.group == 1);
    use_k3s1d1 = use_k3s1d1 && (_param_conv.stride_h == 1);
    use_k3s1d1 = use_k3s1d1 && (_param_conv.stride_w == 1);
    use_k3s1d1 = use_k3s1d1 && (_param_conv.dilation_h == 1);
    use_k3s1d1 = use_k3s1d1 && (_param_conv.dilation_w == 1);
    bool use_depthwise = (Ptype == Precision::FP32);
    use_depthwise = use_depthwise && (_param_conv.group == ins[0]->channel());
    use_depthwise = use_depthwise && (_param_conv.group == outs[0]->channel());
    bool use_direct_k = (Ptype == Precision::FP32);
    use_direct_k = use_direct_k && (_param_conv.weight()->channel() >= 16);
    use_direct_k = use_direct_k && (_param_conv.group == 1);
    if (std::is_same<Ttype, NV>::value
        && (use_k1s1p0 || use_k3s1d1 || use_depthwise || use_direct_k)) {
        impl_e = SABER_IMPL;
    }
#endif
    SABER_CHECK(_funcs_conv.init(ins, outs, _param_conv, SPECIFY, impl_e, ctx));

    if (std::is_same<Ttype, MLU>::value) {
        return Status::OK();
    }
    // check if weights have been transposed
    auto is_weights_transed = CHECK_PARAMETER(is_weights_transed);
    if (!is_weights_transed) {
        SET_PARAMETER(is_weights_transed, true, bool);
        if (bias_term) {
            auto bias = GET_PARAMETER(PBlock<Ttype>, weight_2);
            graph::GraphGlobalMem<Ttype>::Global().template apply<Level_0>(
                    std::bind(&Conv<Ttype, PrecisionWrapper<Ptype>::saber_type>::trans_weights,
                              &_funcs_conv, _1, _2, _3, _4, _5, _6, _7, _8, _9, _10),
                    weights.d_tensor(), bias.d_tensor(), _param_conv.pad_h, _param_conv.pad_w, _param_conv.dilation_h, _param_conv.dilation_w,
                    strides[0], strides[1], group, impl_e);
            bias.map_to_host();
        } else {
            PBlock<Ttype> bias_empty;
            graph::GraphGlobalMem<Ttype>::Global().template apply<Level_0>(
                    std::bind(&Conv<Ttype, PrecisionWrapper<Ptype>::saber_type>::trans_weights,
                              &_funcs_conv, _1, _2, _3, _4, _5, _6, _7, _8, _9, _10),
                    weights.d_tensor(), bias_empty.d_tensor(), _param_conv.pad_h, _param_conv.pad_w, _param_conv.dilation_h, _param_conv.dilation_w,
                    strides[0], strides[1], group, impl_e);
        }
        weights.map_to_host();
    } else {
        PBlock<Ttype> weight_empty;
        PBlock<Ttype> bias_empty;
        graph::GraphGlobalMem<Ttype>::Global().template apply<Level_0>(
                std::bind(&Conv<Ttype, PrecisionWrapper<Ptype>::saber_type>::trans_weights,
                        &_funcs_conv, _1, _2, _3, _4, _5, _6, _7, _8, _9, _10),
                        weight_empty.d_tensor(), bias_empty.d_tensor(), _param_conv.pad_h, _param_conv.pad_w, _param_conv.dilation_h, _param_conv.dilation_w,
                        strides[0], strides[1], group, impl_e);
    }
    return Status::OK();
}

template<typename Ttype, Precision Ptype>
Status ConvolutionHelper<Ttype, Ptype>::InferShape(const
        std::vector<Tensor4dPtr<Ttype> >& ins,
        std::vector<Tensor4dPtr<Ttype> >& outs) {
    SABER_CHECK(_funcs_conv.compute_output_shape(ins, outs, _param_conv));
    return Status::OK();
}

#ifdef USE_CUDA
template class ConvolutionHelper<NV, Precision::FP32>;
template class ConvolutionHelper<NV, Precision::FP16>;
template class ConvolutionHelper<NV, Precision::INT8>;
INSTANCE_CONVOLUTION(NV, Precision::FP32);
INSTANCE_CONVOLUTION(NV, Precision::INT8);
ANAKIN_REGISTER_OP_HELPER(Convolution, ConvolutionHelper, NV, Precision::FP32);
ANAKIN_REGISTER_OP_HELPER(Convolution, ConvolutionHelper, NV, Precision::INT8);

#endif

#ifdef USE_MLU
INSTANCE_CONVOLUTION(MLU, Precision::FP32);
INSTANCE_CONVOLUTION(MLU, Precision::FP16);
template class ConvolutionHelper<MLU, Precision::FP32>;
template class ConvolutionHelper<MLU, Precision::FP16>;
template class ConvolutionHelper<MLU, Precision::INT8>;
ANAKIN_REGISTER_OP_HELPER(Convolution, ConvolutionHelper, MLU, Precision::FP32);
ANAKIN_REGISTER_OP_HELPER(Convolution, ConvolutionHelper, MLU, Precision::FP16);
#endif  // USE_MLU

#if defined USE_X86_PLACE || defined BUILD_LITE
INSTANCE_CONVOLUTION(X86, Precision::FP32);
template class ConvolutionHelper<X86, Precision::FP32>;
ANAKIN_REGISTER_OP_HELPER(Convolution, ConvolutionHelper, X86, Precision::FP32);
INSTANCE_CONVOLUTION(X86, Precision::INT8);
template class ConvolutionHelper<X86, Precision::INT8>;
ANAKIN_REGISTER_OP_HELPER(Convolution, ConvolutionHelper, X86, Precision::INT8);
#endif

#ifdef USE_ARM_PLACE
INSTANCE_CONVOLUTION(ARM, Precision::FP32);
INSTANCE_CONVOLUTION(ARM, Precision::INT8);
template class ConvolutionHelper<ARM, Precision::FP32>;
template class ConvolutionHelper<ARM, Precision::INT8>;
ANAKIN_REGISTER_OP_HELPER(Convolution, ConvolutionHelper, ARM, Precision::FP32);
ANAKIN_REGISTER_OP_HELPER(Convolution, ConvolutionHelper, ARM, Precision::INT8);
#endif

#ifdef AMD_GPU
INSTANCE_CONVOLUTION(AMD, Precision::FP32);
template class ConvolutionHelper<AMD, Precision::FP32>;
template class ConvolutionHelper<AMD, Precision::FP16>;
template class ConvolutionHelper<AMD, Precision::INT8>;
ANAKIN_REGISTER_OP_HELPER(Convolution, ConvolutionHelper, AMD, Precision::FP32);
#endif

//! register op
ANAKIN_REGISTER_OP(Convolution)
.Doc("Convolution operator")
#ifdef USE_CUDA
.__alias__<NV, Precision::FP32>("convolution")
#endif

#ifdef USE_MLU
.__alias__<MLU, Precision::FP32>("convolution")
#endif  // USE_MLU

#ifdef AMD_GPU
.__alias__<AMD, Precision::FP32>("convolution")
#endif
#ifdef USE_ARM_PLACE
.__alias__<ARM, Precision::FP32>("convolution")
.__alias__<ARM, Precision::INT8>("convolution")
#endif
#if defined USE_X86_PLACE || defined BUILD_LITE
.__alias__<X86, Precision::FP32>("convolution")
#endif
.num_in(1)
.num_out(1)
.Args<int>("group", " group of conv ")
.Args<bool>("bias_term", " whether conv weights have bias")
.Args<PTuple<int>>("padding", "padding of conv (x, y)")
.Args<PTuple<int>>("strides", "strides of conv (x)")
.Args<PTuple<int>>("dilation_rate", "dilation rate of conv (x)")
.Args<int>("filter_num", "filter(kernel) number of weights")
.Args<PTuple<int>>("kernel_size", "kernel size of kernel (x, y)")
.Args<int>("axis", "axis of conv");

} /* namespace ops */

} /* namespace anakin */


