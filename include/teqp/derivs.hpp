#pragma once

#include <optional>
#include <complex>
#include <tuple>
#include <map>

#include "MultiComplex/MultiComplex.hpp"

// autodiff include
#include <autodiff/forward/dual.hpp>
#include <autodiff/forward/dual/eigen.hpp>
using namespace autodiff;

template<typename T>
auto forceeval(T&& expr)
{
    using namespace autodiff::detail;
    if constexpr (isDual<T> || isExpr<T> || isNumber<T>) {
        return eval(expr);
    }
    else {
        return expr;
    }
}

/***
* \brief Given a function, use complex step derivatives to calculate the derivative with 
* respect to the first variable which here is temperature
*/
template <typename TType, typename ContainerType, typename FuncType>
typename ContainerType::value_type derivT(const FuncType& f, TType T, const ContainerType& rho) {
    double h = 1e-100;
    return f(std::complex<TType>(T, h), rho).imag() / h;
}

/***
* \brief Given a function, use multicomplex derivatives to calculate the derivative with
* respect to the first variable which here is temperature
*/
template <typename TType, typename ContainerType, typename FuncType>
typename ContainerType::value_type derivTmcx(const FuncType& f, TType T, const ContainerType& rho) {
    using fcn_t = std::function<mcx::MultiComplex<double>(const mcx::MultiComplex<double>&)>;
    fcn_t wrapper = [&rho, &f](const auto& T_) {return f(T_, rho); };
    auto ders = diff_mcx1(wrapper, T, 1);
    return ders[0];
}

/***
* \brief Given a function, use complex step derivatives to calculate the derivative with respect 
* to the given composition variable
*/
template <typename TType, typename ContainerType, typename FuncType, typename Integer>
typename ContainerType::value_type derivrhoi(const FuncType& f, TType T, const ContainerType& rho, Integer i) {
    double h = 1e-100;
    using comtype = std::complex<typename ContainerType::value_type>;
    Eigen::ArrayX<comtype> rhocom(rho.size());
    for (auto j = 0; j < rho.size(); ++j) {
        rhocom[j] = comtype(rho[j], 0.0);
    }
    rhocom[i] = comtype(rho[i], h);
    return f(T, rhocom).imag() / h;
}

enum class ADBackends { autodiff, multicomplex, complex_step };

template<typename Model, typename Scalar = double, typename VectorType = Eigen::ArrayXd>
struct TDXDerivatives {

    static auto get_Ar00(const Model& model, const Scalar& T, const Scalar& rho, const VectorType& molefrac) {
        return model.alphar(T, rho, molefrac);
    }

    template<ADBackends be = ADBackends::autodiff>
    static auto get_Ar10(const Model& model, const Scalar &T, const Scalar &rho, const VectorType& molefrac) {
        if constexpr (be == ADBackends::complex_step) {
            double h = 1e-100;
            return -T * model.alphar(std::complex<Scalar>(T, h), rho, molefrac).imag() / h; // Complex step derivative
        }
        else if constexpr (be == ADBackends::multicomplex) {
            using fcn_t = std::function<mcx::MultiComplex<Scalar>(const mcx::MultiComplex<Scalar>&)>;
            bool and_val = true;
            fcn_t f = [&model, &rho, &molefrac](const auto& Trecip_) { return model.alphar(1.0/Trecip_, rho, molefrac); };
            auto ders = diff_mcx1(f, 1.0/T, 1, and_val);
            return (1.0/T)*ders[1];
        }
        else if constexpr (be == ADBackends::autodiff) {
            autodiff::dual Trecipdual = 1.0/T;
            auto f = [&model, &rho, &molefrac](const auto& Trecip_) { return eval(model.alphar(eval(1.0/Trecip_), rho, molefrac)); };
            auto der = derivative(f, wrt(Trecipdual), at(Trecipdual));
            return (1.0/T)*der;
        }
        else {
            static_assert("algorithmic differentiation backend is invalid in get_Ar10");
        }
    }

    template<ADBackends be = ADBackends::autodiff>
    static auto get_Ar01(const Model& model, const Scalar&T, const Scalar &rho, const VectorType& molefrac) {
        if constexpr(be == ADBackends::complex_step){
            double h = 1e-100;
            auto der = model.alphar(T, std::complex<Scalar>(rho, h), molefrac).imag() / h;
            return rho*der;
        }
        else if constexpr(be == ADBackends::multicomplex){
            using fcn_t = std::function<mcx::MultiComplex<Scalar>(const mcx::MultiComplex<Scalar>&)>;
            bool and_val = true;
            fcn_t f = [&model, &T, &molefrac](const auto& rho_) { return model.alphar(T, rho_, molefrac); };
            auto ders = diff_mcx1(f, rho, 1, and_val);
            return rho*ders[1];
        }
        else if constexpr(be == ADBackends::autodiff){
            autodiff::dual rhodual = rho;
            auto f = [&model, &T, &molefrac](const auto& rho_) { return eval(model.alphar(T, rho_, molefrac)); };
            auto der = derivative(f, wrt(rhodual), at(rhodual));
            return rho*der;
        }
        else {
            static_assert("algorithmic differentiation backend is invalid in get_Ar01");
        }
    }

    template<ADBackends be = ADBackends::autodiff>
    static auto get_Ar02(const Model& model, const Scalar& T, const Scalar& rho, const VectorType& molefrac) {
        if constexpr (be == ADBackends::autodiff) {
            autodiff::dual2nd rhodual = rho;
            auto f = [&model, &T, &molefrac](const auto& rho_) { return eval(model.alphar(T, rho_, molefrac)); };
            auto ders = derivatives(f, wrt(rhodual), at(rhodual));
            return rho*rho*ders[2];
        }
        else {
            static_assert("algorithmic differentiation backend is invalid in get_Ar02");
        }
    }

    template<int Nderiv, ADBackends be = ADBackends::autodiff>
    static auto get_Ar0n(const Model& model, const Scalar& T, const Scalar& rho, const VectorType& molefrac) {
        std::map<int, double> o;
        if constexpr (be == ADBackends::autodiff) {
            autodiff::HigherOrderDual<Nderiv, double> rhodual = rho;
            auto f = [&model, &T, &molefrac](const auto& rho_) { return eval(model.alphar(T, rho_, molefrac)); };
            auto ders = derivatives(f, wrt(rhodual), at(rhodual));
            for (auto n = 1; n <= Nderiv; ++n) {
                o[n] = pow(rho, n)*ders[n];
            }
            return o;
        }
        else {
            static_assert("algorithmic differentiation backend is invalid in get_Ar0n");
        }
    }

    template<ADBackends be = ADBackends::autodiff>
    static auto get_Ar20(const Model& model, const Scalar& T, const Scalar& rho, const VectorType& molefrac) {
        if constexpr (be == ADBackends::autodiff) {
            autodiff::dual2nd Trecipdual = 1/T;
            auto f = [&model, &rho, &molefrac](const auto& Trecip) { return eval(model.alphar(eval(1/Trecip), rho, molefrac)); };
            auto ders = derivatives(f, wrt(Trecipdual), at(Trecipdual));
            return (1/T)*(1/T)*ders[2];
        }
        else {
            static_assert("algorithmic differentiation backend is invalid in get_Ar20");
        }
    }

    template<ADBackends be = ADBackends::autodiff>
    static auto get_Ar11(const Model& model, const Scalar& T, const Scalar& rho, const VectorType& molefrac) {

        if constexpr (be == ADBackends::multicomplex) {
            using fcn_t = std::function< mcx::MultiComplex<double>(const std::valarray<mcx::MultiComplex<double>>&)>;
            const fcn_t func = [&model, &molefrac](const auto& zs) {
                auto rhomolar = zs[0], Trecip = zs[1];
                return model.alphar(1.0/Trecip, rhomolar, molefrac);
            };
            std::vector<double> xs = { rho, 1.0/T};
            std::vector<int> order = { 1, 1};
            auto der = mcx::diff_mcxN(func, xs, order);
            return (1.0/T)*rho*der;
        }
        else if constexpr (be == ADBackends::autodiff) {
            autodiff::dual2nd rhodual = rho, Trecipdual=1/T;
            auto f = [&model, &molefrac](const auto& Trecip, const auto&rho_) { return eval(model.alphar(eval(1/Trecip), rho_, molefrac)); };
            //auto der = derivative(f, wrt(Trecipdual, rhodual), at(Trecipdual, rhodual)); // d^2alphar/drhod(1/T) // This should work, but gives instead 1,0 derivative
            auto [u01, u10, u11] = derivatives(f, wrt(Trecipdual, rhodual), at(Trecipdual, rhodual)); // d^2alphar/drhod(1/T)
            return (1.0/T)*rho*u11;
        }
        else {
            static_assert("algorithmic differentiation backend is invalid in get_Ar11");
        }
    }

    template<ADBackends be = ADBackends::autodiff>
    static auto get_Ar12(const Model& model, const Scalar& T, const Scalar& rho, const VectorType& molefrac) {

        if constexpr (be == ADBackends::multicomplex) {
            using fcn_t = std::function< mcx::MultiComplex<double>(const std::valarray<mcx::MultiComplex<double>>&)>;
            const fcn_t func = [&model, &molefrac](const auto& zs) {
                auto rhomolar = zs[0], Trecip = zs[1];
                return model.alphar(1.0 / Trecip, rhomolar, molefrac);
            };
            std::vector<double> xs = { rho, 1.0/T };
            std::vector<int> order = { 1, 2 };
            auto der = mcx::diff_mcxN(func, xs, order);
            return (1.0/T)*rho*rho*der;
        }
        else if constexpr (be == ADBackends::autodiff) {
            //static_assert("bug in autodiff, can't use autodiff for cross derivative");
            autodiff::dual3rd rhodual = rho, Trecipdual = 1 / T;
            auto f = [&model, &molefrac](const auto& Trecip, const auto& rho_) { return eval(model.alphar(eval(1 / Trecip), rho_, molefrac)); };
            auto ders = derivatives(f, wrt(Trecipdual, rhodual, rhodual), at(Trecipdual, rhodual));
            return (1.0/T)*rho*rho*ders.back();
        }
        else {
            static_assert("algorithmic differentiation backend is invalid in get_Ar12");
        }
    }

    template<ADBackends be = ADBackends::autodiff>
    static auto get_neff(const Model& model, const Scalar& T, const Scalar& rho, const VectorType& molefrac) {
        auto Ar01 = get_Ar01<be>(model, T, rho, molefrac);
        auto Ar11 = get_Ar11<be>(model, T, rho, molefrac);
        auto Ar20 = get_Ar20(model, T, rho, molefrac);
        return -3*(Ar01-Ar11)/Ar20;
    }
};

template<typename Model, typename Scalar = double, typename VectorType = Eigen::ArrayXd>
struct VirialDerivatives {

    static auto get_B2vir(const Model& model, const Scalar &T, const VectorType& molefrac) {
        Scalar h = 1e-100;
        // B_2 = dalphar/drho|T,z at rho=0
        auto B2 = model.alphar(T, std::complex<Scalar>(0.0, h), molefrac).imag()/h;
        return B2;
    }

    /**
    * \f$
    * B_n = \frac{1}{(n-2)!} lim_rho\to 0 d^{n-1}alphar/drho^{n-1}|T,z
    * \f$
    * \param model The model providing the alphar function
    * \param Nderiv The maximum virial coefficient to return; e.g. 5: B_2, B_3, ..., B_5
    * \param T Temperature
    * \param molefrac The mole fractions
    */

    template <int Nderiv, ADBackends be = ADBackends::autodiff>
    static auto get_Bnvir(const Model& model, const Scalar &T, const VectorType& molefrac) 
    {
        std::map<int, double> dnalphardrhon;
        if constexpr(be == ADBackends::multicomplex){
            using namespace mcx;
            using fcn_t = std::function<MultiComplex<double>(const MultiComplex<double>&)>;
            fcn_t f = [&model, &T, &molefrac](const auto& rho_) { return model.alphar(T, rho_, molefrac); };
            auto derivs = diff_mcx1(f, 0.0, Nderiv+1, true /* and_val */);
            for (auto n = 1; n <= Nderiv; ++n){
                dnalphardrhon[n] = derivs[n];
            }
        }
        else if constexpr(be == ADBackends::autodiff){
            autodiff::HigherOrderDual<Nderiv+1, double> rhodual = 0.0;
            auto f = [&model, &T, &molefrac](const auto& rho_) { return model.alphar(T, rho_, molefrac); };
            auto derivs = derivatives(f, wrt(rhodual), at(rhodual));
            for (auto n = 1; n <= Nderiv; ++n){
                 dnalphardrhon[n] = derivs[n];
            }
        }
        else{
            static_assert("algorithmic differentiation backend is invalid");
        }
        std::map<int, Scalar> o;
        for (int n = 2; n < Nderiv+1; ++n) {
            o[n] = dnalphardrhon[n-1];
            // 0!=1, 1!=1, so only n>3 terms need factorial correction
            if (n > 3) {
                auto factorial = [](int N) {return tgamma(N + 1); };
                o[n] /= factorial(n-2);
            }
        }
        return o;
    }

    static auto get_B12vir(const Model& model, const Scalar &T, const VectorType& molefrac) {
    
        auto B2 = get_B2vir(model, T, molefrac); // Overall B2 for mixture
        const auto xpure0 = (Eigen::ArrayXd(2) << 1,0).finished();
        const auto xpure1 = (Eigen::ArrayXd(2) << 0,1).finished();
        auto B20 = get_B2vir(model, T, xpure0); // Pure first component with index 0
        auto B21 = get_B2vir(model, T, xpure1); // Pure second component with index 1
        auto z0 = molefrac[0];
        auto B12 = (B2 - z0*z0*B20 - (1-z0)*(1-z0)*B21)/(2*z0*(1-z0));
        return B12;
    }
};


template<typename Model, typename Scalar = double, typename VectorType = Eigen::ArrayXd>
struct IsochoricDerivatives{

    /***
    * \brief Calculate the residual entropy (s^+ = -sr/R) from derivatives of alphar
    */
    static auto get_splus(const Model& model, const Scalar &T, const VectorType& rhovec) {
        auto rhotot = rhovec.sum();
        auto molefrac = rhovec / rhotot;
        return model.alphar(T, rhotot, molefrac) - get_Ar10(model, T, rhovec);
    }

    /***
    * \brief Calculate the residual pressure from derivatives of alphar
    */
    static auto get_pr(const Model& model, const Scalar &T, const VectorType& rhovec)
    {
        auto rhotot_ = rhovec.sum();
        auto molefrac = (rhovec / rhotot_).eval();
        auto h = 1e-100;
        auto Ar01 = model.alphar(T, std::complex<double>(rhotot_, h), molefrac).imag() / h * rhotot_;
        return Ar01*rhotot_*model.R*T;
    }

    static auto get_Ar00(const Model& model, const Scalar& T, const VectorType& rhovec) {
        auto rhotot = rhovec.sum();
        auto molefrac = (rhovec / rhotot).eval();
        return model.alphar(T, rhotot, molefrac);
    }

    static auto get_Ar10(const Model& model, const Scalar& T, const VectorType& rhovec) {
        auto rhotot = rhovec.sum();
        auto molefrac = (rhovec / rhotot).eval();
        return -T * derivT([&model, &rhotot, &molefrac](const auto& T, const auto& rhovec) { return model.alphar(T, rhotot, molefrac); }, T, rhovec);
    }

    static auto get_Ar01(const Model& model, const Scalar &T, const VectorType& rhovec) {
        auto rhotot_ = std::accumulate(std::begin(rhovec), std::end(rhovec), (decltype(rhovec[0]))0.0);
        decltype(rhovec[0] * T) Ar01 = 0.0;
        for (auto i = 0; i < rhovec.size(); ++i) {
            auto Ar00 = [&model](const auto &T, const auto&rhovec) {
                auto rhotot = rhovec.sum();
                auto molefrac = rhovec / rhotot;
                return model.alphar(T, rhotot, molefrac);
            };
            Ar01 += rhovec[i] * derivrhoi(Ar00, T, rhovec, i);
        }
        return Ar01;
    }

    /***
    * \brief Calculate Psir=ar*rho
    */
    static auto get_Psir(const Model& model, const Scalar &T, const VectorType& rhovec) {
        auto rhotot_ = std::accumulate(std::begin(rhovec), std::end(rhovec), (decltype(rhovec[0]))0.0);
        return model.alphar(T, rhotot_, rhovec / rhotot_) * model.R * T * rhotot_;
    }

    /***
    * \brief Calculate the Hessian of Psir = ar*rho w.r.t. the molar concentrations
    *
    * Requires the use of autodiff derivatives to calculate second partial derivatives
    */
    static auto build_Psir_Hessian_autodiff(const Model& model, const Scalar& T, const VectorType& rho) {
        // Double derivatives in each component's concentration
        // N^N matrix (symmetric)

        dual2nd u; // the output scalar u = f(x), evaluated together with Hessian below
        ArrayXdual2nd g;
        ArrayXdual2nd rhovecc(rho.size()); for (auto i = 0; i < rho.size(); ++i) { rhovecc[i] = rho[i]; }
        auto hfunc = [&model, &T](const ArrayXdual2nd& rho_) {
            auto rhotot_ = rho_.sum();
            auto molefrac = (rho_ / rhotot_).eval();
            return eval(model.alphar(T, rhotot_, molefrac) * model.R * T * rhotot_);
        };
        return autodiff::hessian(hfunc, wrt(rhovecc), at(rhovecc), u, g).eval(); // evaluate the function value u, its gradient, and its Hessian matrix H
    }

    /***
    * \brief Calculate the Hessian of Psi = a*rho w.r.t. the molar concentrations
    *
    * Uses autodiff derivatives to calculate second partial derivatives
    */
    static auto build_Psi_Hessian_autodiff(const Model& model, const Scalar& T, const VectorType& rho) {
        auto H = build_Psir_Hessian_autodiff(model, T, rho);
        for (auto i = 0; i < 2; ++i) {
            H(i, i) += model.R * T / rho[i];
        }
        return H;
    }

    /***
    * \brief Calculate the Hessian of Psir = ar*rho w.r.t. the molar concentrations (residual contribution only)
    *
    * Requires the use of multicomplex derivatives to calculate second partial derivatives
    */
    static auto build_Psir_Hessian_mcx(const Model& model, const Scalar& T, const VectorType& rho) {
        // Double derivatives in each component's concentration
        // N^N matrix (symmetric)
        using namespace mcx;

        // Lambda function for getting Psir with multicomplex concentrations
        using fcn_t = std::function< MultiComplex<double>(const Eigen::ArrayX<MultiComplex<double>>&)>;
        fcn_t func = [&model, &T](const auto& rhovec) {
            auto rhotot_ = rhovec.sum();
            auto molefrac = (rhovec / rhotot_).eval();
            return model.alphar(T, rhotot_, molefrac) * model.R * T * rhotot_;
        };
        using mattype = Eigen::ArrayXXd;
        auto H = get_Hessian<mattype, fcn_t, VectorType, HessianMethods::Multiple>(func, rho);
        return H;
    }

    /***
    * \brief Gradient of Psir = ar*rho w.r.t. the molar concentrations
    *
    * Uses autodiff to calculate second partial derivatives
    */
    static auto build_Psir_gradient_autodiff(const Model& model, const Scalar& T, const VectorType& rho) {
        ArrayXdual2nd rhovecc(rho.size()); for (auto i = 0; i < rho.size(); ++i) { rhovecc[i] = rho[i]; }
        auto psirfunc = [&model, &T](const ArrayXdual2nd& rho_) {
            auto rhotot_ = rho_.sum();
            auto molefrac = (rho_ / rhotot_).eval();
            return eval(model.alphar(T, rhotot_, molefrac) * model.R * T * rhotot_);
        };
        auto val = autodiff::gradient(psirfunc, wrt(rhovecc), at(rhovecc)).eval(); // evaluate the gradient
        return val;
    }

    /***
    * \brief Calculate the chemical potential of each component
    *
    * Uses autodiff derivatives to calculate second partial derivatives
    * See Eq. 9 of https://doi.org/10.1002/aic.16730
    * \note: Some contributions to the ideal gas part are missing (reference state and cp0), but are not relevant to phase equilibria
    */
    static auto get_chempot_autodiff(const Model& model, const Scalar& T, const VectorType& rho) {
        typename VectorType::value_type rhotot = rho.sum();
        return (build_Psir_gradient_autodiff(model, T, rho).array() + model.R*T*(1.0 + log(rho / rhotot))).eval();
    }
};