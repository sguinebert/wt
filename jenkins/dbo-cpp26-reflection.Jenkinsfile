properties([
    pipelineTriggers([
        pollSCM('@midnight')
    ])
])

node('wt-cpp26-reflection') {
    try {
        stage('Checkout') {
            checkout scm
        }

        stage('Toolchain') {
            sh 'cmake --version'
            sh 'clang++ --version || true'
            sh 'c++ --version || true'
        }

        stage('Wt::Dbo throw/catch budget') {
            sh './tools/ci/check_wtdbo_throw_catch_budget.sh'
        }

        stage('Configure (wtdbo reflection hard cut)') {
            sh """cmake -S . -B build-dbo-reflection -G Ninja \
                    -DCMAKE_BUILD_TYPE=RelWithDebInfo \
                    -DCMAKE_C_COMPILER=clang \
                    -DCMAKE_CXX_COMPILER=clang++ \
                    -DENABLE_LIBWTDBO=ON \
                    -DWT_DBO_CPP26_HARD_CUT=ON \
                    -DBUILD_EXAMPLES=OFF \
                    -DBUILD_TESTS=OFF"""
        }

        stage('Build wtdbo') {
            sh 'cmake --build build-dbo-reflection --target wtdbo -j$(nproc)'
        }
    } finally {
        cleanWs()
    }
}
