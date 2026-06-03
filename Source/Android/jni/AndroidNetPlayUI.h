#pragma once

#include "Core/NetPlayClient.h"

// Minimal NetPlayUI implementation for Android
class AndroidNetPlayUI : public NetPlay::NetPlayUI {
public:
  void BootGame(const std::string& filename,
                std::unique_ptr<BootSessionData> boot_session_data) override;
  bool IsHosting() const override;
  void Update() override;
  void AppendChat(const std::string& msg) override;
  void OnMsgChangeGame(const NetPlay::SyncIdentifier& sync_identifier,
                       const std::string& netplay_name) override;
  void OnMsgChangeGBARom(int pad, const NetPlay::GBAConfig& config) override;
  void OnMsgStartGame() override;
  void OnMsgStopGame() override;
  void OnMsgPowerButton() override;
  void OnPlayerConnect(const std::string& player) override;
  void OnPlayerDisconnect(const std::string& player) override;
  void OnPadBufferChanged(u32 buffer) override;
  void OnHostInputAuthorityChanged(bool enabled) override;
  void OnDesync(u32 frame, const std::string& player) override;
  void OnConnectionLost() override;
  void OnConnectionError(const std::string& message) override;
  void OnTraversalError(
      Common::TraversalClient::FailureReason error) override;
  void OnTraversalStateChanged(Common::TraversalClient::State state) override;
  void OnGameStartAborted() override;
  void OnGolferChanged(bool is_golfer, const std::string& golfer_name) override;
  void OnTtlDetermined(u8 ttl) override;
  bool IsRecording() override;
  std::string FindGBARomPath(const std::array<u8, 20>& hash,
                             std::string_view title, int device_number) override;
  void ShowGameDigestDialog(const std::string& title) override;
  void SetGameDigestProgress(int pid, int progress) override;
  void SetGameDigestResult(int pid, const std::string& result) override;
  void AbortGameDigest() override;
  void OnIndexAdded(bool success, std::string error) override;
  void OnIndexRefreshFailed(std::string error) override;
  void ShowChunkedProgressDialog(const std::string& title, u64 data_size,
                                 const std::vector<int>& players) override;
  void HideChunkedProgressDialog() override;
  void SetChunkedProgress(int pid, u64 progress) override;
  void SetHostWiiSyncData(std::vector<u64> titles,
                          std::string redirect_folder) override;
  void StopGame() override;
  std::shared_ptr<const UICommon::GameFile>
  FindGameFile(const NetPlay::SyncIdentifier& sync_identifier,
               NetPlay::SyncIdentifierComparison* found = nullptr) override;
};
