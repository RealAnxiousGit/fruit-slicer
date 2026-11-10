package com.realanxiousgit.fruitslicer;

import android.content.ContentProvider;
import android.content.ContentValues;
import android.database.Cursor;
import android.database.MatrixCursor;
import android.net.Uri;
import android.os.ParcelFileDescriptor;
import android.provider.OpenableColumns;

import java.io.File;
import java.io.FileNotFoundException;

// Lends a score card picture, read-only, to the app the player shares it with. Only files directly in the
// game's own cards folder can be reached; the game keeps just the newest one there.
public class CardProvider extends ContentProvider {
    public static final String AUTHORITY = "com.realanxiousgit.fruitslicer.cards";

    private File card(Uri uri) throws FileNotFoundException {
        String name = uri.getLastPathSegment();
        if (name == null || name.contains("/") || name.startsWith(".") || getContext() == null) throw new FileNotFoundException();
        File file = new File(new File(getContext().getFilesDir(), "cards"), name);
        if (!file.isFile()) throw new FileNotFoundException();
        return file;
    }

    @Override
    public boolean onCreate() {
        return true;
    }

    @Override
    public ParcelFileDescriptor openFile(Uri uri, String mode) throws FileNotFoundException {
        if (!"r".equals(mode)) throw new FileNotFoundException();
        return ParcelFileDescriptor.open(card(uri), ParcelFileDescriptor.MODE_READ_ONLY);
    }

    @Override
    public String getType(Uri uri) {
        return "image/png";
    }

    // Apps ask for the file's name and size before reading it.
    @Override
    public Cursor query(Uri uri, String[] projection, String selection, String[] args, String sort) {
        File file;
        try {
            file = card(uri);
        } catch (FileNotFoundException e) {
            return null;
        }
        String[] cols = projection != null ? projection : new String[] {OpenableColumns.DISPLAY_NAME, OpenableColumns.SIZE};
        MatrixCursor cursor = new MatrixCursor(cols, 1);
        Object[] row = new Object[cols.length];
        for (int i = 0; i < cols.length; i++) {
            if (OpenableColumns.DISPLAY_NAME.equals(cols[i])) row[i] = file.getName();
            else if (OpenableColumns.SIZE.equals(cols[i])) row[i] = file.length();
        }
        cursor.addRow(row);
        return cursor;
    }

    @Override
    public Uri insert(Uri uri, ContentValues values) {
        return null;
    }

    @Override
    public int delete(Uri uri, String selection, String[] args) {
        return 0;
    }

    @Override
    public int update(Uri uri, ContentValues values, String selection, String[] args) {
        return 0;
    }
}
