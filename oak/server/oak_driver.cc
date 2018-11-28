#include "absl/strings/str_split.h"
#include "absl/synchronization/notification.h"
#include "absl/time/time.h"
#include "asylo/client.h"
#include "asylo/grpc/util/enclave_server.pb.h"
#include "asylo/util/logging.h"
#include "gflags/gflags.h"

#include "oak/proto/oak.pb.h"

#include <fstream>
#include <iostream>
#include <string>
#include <vector>

DEFINE_string(enclave_path, "", "Path to enclave to load");

int main(int argc, char *argv[]) {
  // Setup.
  ::google::ParseCommandLineFlags(&argc, &argv, /*remove_flags=*/true);

  // Initialise enclave.
  asylo::EnclaveManager::Configure(asylo::EnclaveManagerOptions());
  auto manager_result = asylo::EnclaveManager::Instance();
  if (!manager_result.ok()) {
    LOG(QFATAL) << "EnclaveManager unavailable: " << manager_result.status();
  }
  asylo::EnclaveManager *manager = manager_result.ValueOrDie();
  std::cout << "Loading " << FLAGS_enclave_path << std::endl;
  asylo::SimLoader loader(FLAGS_enclave_path, /*debug=*/true);

  // Loading.
  {
    asylo::EnclaveConfig config;
    asylo::ServerConfig *server_config = config.MutableExtension(asylo::server_input_config);
    server_config->set_host("0.0.0.0");
    server_config->set_port(8888);
    asylo::Status status = manager->LoadEnclave("oak_enclave", loader, config);
    if (!status.ok()) {
      LOG(QFATAL) << "Load " << FLAGS_enclave_path << " failed: " << status;
    }
  }

  asylo::EnclaveClient *client = manager->GetClient("oak_enclave");

  // Initialisation.
  // This does not actually do anything interesting at the moment.
  {
    LOG(INFO) << "Initialising enclave";
    asylo::EnclaveInput input;
    asylo::EnclaveOutput output;
    asylo::Status status = client->EnterAndRun(input, &output);
    if (!status.ok()) {
      LOG(QFATAL) << "EnterAndRun failed: " << status;
    }
    LOG(INFO) << "Enclave initialised";
  }

  // Wait.
  {
    absl::Notification server_timeout;
    server_timeout.WaitForNotificationWithTimeout(absl::Seconds(300));
  }

  // Finalisation.
  {
    LOG(INFO) << "Destroying enclave";
    asylo::EnclaveFinal final_input;
    asylo::Status status = manager->DestroyEnclave(client, final_input);
    if (!status.ok()) {
      LOG(QFATAL) << "Destroy " << FLAGS_enclave_path << " failed: " << status;
    }
    LOG(INFO) << "Enclave destroyed";
  }

  return 0;
}
