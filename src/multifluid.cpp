#define USE_AUTODIFF

#include "teqp/core.hpp"
#include "teqp/models/multifluid.hpp"

#include <optional>

class Timer {
private:
    int N;
    decltype(std::chrono::steady_clock::now()) tic;
public:
    Timer(int N) : N(N), tic(std::chrono::steady_clock::now()){}
    ~Timer() {
        auto elap = std::chrono::duration<double>(std::chrono::steady_clock::now()-tic).count();
        std::cout << elap/N*1e6 << " us/call" << std::endl;
    }
};

void trace_critical_loci(const std::string &coolprop_root, const nlohmann::json &BIPcollection) {
    std::vector<std::vector<std::string>> pairs = { 
        { "CarbonDioxide", "R1234YF" }, { "CarbonDioxide","R1234ZE(E)" }, { "ETHYLENE","R1243ZF" }, 
        { "R1234YF","R1234ZE(E)" }, { "R134A","R1234YF" }, { "R23","R1234YF" }, 
        { "R32","R1123" }, { "R32","R1234YF" }, { "R32","R1234ZE(E)" }
    };
    for (auto &pp : pairs) {
        using ModelType = decltype(build_multifluid_model(pp, coolprop_root, BIPcollection));
        std::optional<ModelType> optmodel{std::nullopt};
        try {
            optmodel.emplace(build_multifluid_model(pp, coolprop_root, BIPcollection));
        }
        catch (std::exception &e) {
            std::cout << e.what() << std::endl;
            std::cout << pp[0] << "&" << pp[1] << std::endl;
            continue;
        }
        for (int i : {0, 1}){
            const auto &model = optmodel.value();
            auto rhoc0 = 1.0 / model.redfunc.vc[i];
            auto T0 = model.redfunc.Tc[i];
            std::valarray<double> rhovec(2); rhovec[i] = { rhoc0 }; rhovec[1L - i] = 0.0;
            // Non-analytic terms make it impossible to initialize AT the pure components
            if (pp[0] == "CarbonDioxide" || pp[1] == "CarbonDioxide") {
                if (i == 0) {
                    rhovec[i] *= 0.9999;
                    rhovec[1L - i] = 0.9999;
                }
                else {
                    rhovec[i] *= 1.0001;
                    rhovec[1L - i] = 1.0001;
                }
                double zi = rhovec[i] / rhovec.sum();
                double T = zi * model.redfunc.Tc[i] + (1 - zi) * model.redfunc.Tc[1L - i];
                double z0 = (i == 0) ? zi : 1-zi;
                auto [Tnew, rhonew] = critical_polish_molefrac(model, T, rhovec, z0);
                T0 = Tnew;
                rhoc0 = rhovec.sum();
            }
            std::string filename = pp[0] + "_" + pp[1] + ".csv";
            trace_critical_arclength_binary(model, rhovec, T0, filename);
        }
    }
}

int main(){
   
    std::string coolprop_root = "C:/Users/ihb/Code/CoolProp";
    coolprop_root = "../mycp";
    auto BIPcollection = nlohmann::json::parse(
        std::ifstream(coolprop_root + "/dev/mixtures/mixture_binary_pairs.json")
    );

    // Critical curves
    {
        Timer t(1);
        trace_critical_loci(coolprop_root, BIPcollection);
    }

{
    auto model = build_multifluid_model({ "methane", "ethane" }, coolprop_root, BIPcollection);
    std::valarray<double> rhovec = { 1.0, 2.0 };
    double T = 300;
    {
        const std::valarray<double> molefrac = { rhovec[0]/rhovec.sum(), rhovec[1]/rhovec.sum() };
        const double rho = rhovec.sum();
        volatile double T = 300.0;
        constexpr int N = 10000;
        volatile double alphar;
        double rrrr = get_Ar01(model, T, rho, molefrac);
        double rrrr2 = get_Ar02(model, T, rho, molefrac);
        {
            Timer t(N);
            for (auto i = 0; i < N; ++i){
                alphar = model.alphar(T, rho, molefrac);
            }
            std::cout << alphar << std::endl;
        }
        {
            Timer t(N);
            for (auto i = 0; i < N; ++i) {
                alphar = get_Ar01<ADBackends::complex_step>(model, T, rho, molefrac);
            }
            std::cout << alphar << "; 1st CSD" << std::endl;
        }
        {
            Timer t(N);
            for (auto i = 0; i < N; ++i) {
                alphar = get_Ar01<ADBackends::autodiff>(model, T, rho, molefrac);
            }
            std::cout << alphar << "; 1st autodiff::autodiff" << std::endl;
        }
        {
            Timer t(N);
            for (auto i = 0; i < N; ++i) {
                alphar = get_Ar01<ADBackends::multicomplex>(model, T, rho, molefrac);
            }
            std::cout << alphar << "; 1st MCX" << std::endl;
        }
        {
            Timer t(N);
            for (auto i = 0; i < N; ++i) {
                alphar = get_Ar02(model, T, rho, molefrac);
            }
            std::cout << alphar << std::endl;
        }
    }

    auto alphar = model.alphar(T, rhovec);
    auto Ar01 = get_Ar01(model, T, rhovec);
    auto Ar10 = get_Ar10(model, T, rhovec);
    auto splus = get_splus(model, T, rhovec);

    std::valarray<double> molefrac = { 1.0/3.0, 2.0/3.0 };
    auto B2 = get_B2vir(model, T, molefrac);

    std::valarray<double> dilrho = 0.00000000001*molefrac;
    auto B2other = get_Ar01(model, T, dilrho)/dilrho.sum();

    std::valarray<double> zerorho = 0.0*rhovec;
    auto Ar01dil = get_Ar01(model, T, zerorho);
    
    int ttt =0 ;
}
    return EXIT_SUCCESS;
}
