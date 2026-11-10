package com.realanxiousgit.fruitslicer;

import android.app.Activity;
import android.app.PendingIntent;
import android.content.ClipData;
import android.content.Intent;
import android.content.pm.PackageInstaller;
import android.net.Uri;
import android.os.Build;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.InputStream;
import java.io.OutputStream;
import java.net.HttpURLConnection;
import java.net.URL;

import org.libsdl.app.SDL;
import org.libsdl.app.SDLActivity;

public class FruitSlicerActivity extends SDLActivity {
    private static final String ACTION_INSTALL_STATUS = "com.realanxiousgit.fruitslicer.INSTALL_STATUS";

    // Updates (port/update_port.cpp): fetches one file to `path`, following GitHub's redirects.
    // Runs on the game's update thread, never the UI thread.
    public static boolean fetch(String url, String path) {
        HttpURLConnection c = null;
        try {
            c = (HttpURLConnection) new URL(url).openConnection();
            c.setRequestProperty("User-Agent", "FruitSlicer");
            c.setConnectTimeout(20000);
            c.setReadTimeout(30000);
            if (c.getResponseCode() != 200) return false;
            try (InputStream in = c.getInputStream(); OutputStream out = new FileOutputStream(path)) {
                byte[] buf = new byte[64 * 1024];
                int n;
                while ((n = in.read(buf)) > 0) out.write(buf, 0, n);
            }
            return true;
        } catch (Exception e) {
            return false;
        } finally {
            if (c != null) c.disconnect();
        }
    }

    // Hands a downloaded APK to Android's package installer, which asks the player to confirm. It must be
    // signed with the same key as the installed game, or Android refuses it.
    public static boolean installApk(String path) {
        try {
            Activity ctx = SDL.getContext();
            PackageInstaller installer = ctx.getPackageManager().getPackageInstaller();
            PackageInstaller.SessionParams params = new PackageInstaller.SessionParams(PackageInstaller.SessionParams.MODE_FULL_INSTALL);
            int id = installer.createSession(params);
            File apk = new File(path);
            try (PackageInstaller.Session session = installer.openSession(id)) {
                try (InputStream in = new FileInputStream(apk); OutputStream out = session.openWrite("game.apk", 0, apk.length())) {
                    byte[] buf = new byte[64 * 1024];
                    int n;
                    while ((n = in.read(buf)) > 0) out.write(buf, 0, n);
                    session.fsync(out);
                }
                Intent status = new Intent(ctx, FruitSlicerActivity.class).setAction(ACTION_INSTALL_STATUS);
                int flags = PendingIntent.FLAG_UPDATE_CURRENT | (Build.VERSION.SDK_INT >= 31 ? PendingIntent.FLAG_MUTABLE : 0);
                session.commit(PendingIntent.getActivity(ctx, id, status, flags).getIntentSender());
            }
            apk.delete();
            return true;
        } catch (Exception e) {
            return false;
        }
    }

    // Score cards (port/port_main.cpp): offers the picture to any app that takes one. CardProvider lends
    // the file only to the app the player picks, so no storage permission is needed.
    public static boolean shareCard(String path) {
        try {
            Activity ctx = SDL.getContext();
            File file = new File(path);
            Uri uri = Uri.parse("content://" + CardProvider.AUTHORITY + "/" + Uri.encode(file.getName()));
            Intent send = new Intent(Intent.ACTION_SEND);
            send.setType("image/png");
            send.putExtra(Intent.EXTRA_STREAM, uri);
            send.setClipData(ClipData.newRawUri("", uri));
            send.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION);
            Intent chooser = Intent.createChooser(send, "Share your score");
            chooser.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION);
            ctx.startActivity(chooser);
            return true;
        } catch (Exception e) {
            return false;
        }
    }

    // The installer reports back here; when it needs the player's OK, its confirmation screen is shown.
    @Override
    protected void onNewIntent(Intent intent) {
        super.onNewIntent(intent);
        if (intent == null || !ACTION_INSTALL_STATUS.equals(intent.getAction())) return;
        int status = intent.getIntExtra(PackageInstaller.EXTRA_STATUS, PackageInstaller.STATUS_FAILURE);
        if (status == PackageInstaller.STATUS_PENDING_USER_ACTION) {
            Intent confirm = intent.getParcelableExtra(Intent.EXTRA_INTENT);
            if (confirm != null) startActivity(confirm);
        }
    }
}
